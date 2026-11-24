#include "compiler/compiler.h"
#include "workspace.h"

#include <Windows.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#include <wchar.h>

static HANDLE create_compiler_job(void) {
  HANDLE job = CreateJobObjectW(NULL, NULL);

  if (job == NULL) {
    return NULL;
  }

  // ハンドルを閉じると Clang とその子プロセスも止まるようにする。
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {0};

  info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

  BOOL configured =
      SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info));

  if (!configured) {
    DWORD error = GetLastError();
    CloseHandle(job);
    SetLastError(error);
    return NULL;
  }

  return job;
}

// resultが占有する動的メモリの解放処理
void compile_result_free(CompileResult *result) {
  // resultが空のとき、何もせず返す
  if (result == NULL) {
    return;
  }

  // メモリ解放
  free(result->diagnostics);
  free(result->output_path);

  // 解放後NULL値を代入
  result->diagnostics = NULL;
  result->output_path = NULL;
}

#define LOG_LIMIT (1024u * 1024u)
#define WAIT_SLICE_MS 10u

static void set_system_error(CompileResult *result, DWORD error) {
  wchar_t message[512];
  DWORD length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                                error, 0, message, _countof(message), NULL);
  if (length == 0) {
    StringCchPrintfW(message, _countof(message), L"Windows error %lu", error);
  }

  char utf8[sizeof(message) * 2];
  if (WideCharToMultiByte(CP_UTF8, 0, message, -1, utf8, sizeof(utf8), NULL, NULL)) {
    char *copy = _strdup(utf8);
    if (copy != NULL) {
      free(result->diagnostics);
      result->diagnostics = copy;
    }
  }
}

// Windows の UTF-8 デコーダーで不正なシーケンスを U+FFFD に置換する。
static int set_diagnostics(CompileResult *result, char *bytes, size_t length) {
  if (length == 0) {
    return 1;
  }
  if (result->logs_truncated) {
    size_t start = length - 1;
    while (start > 0 && length - start < 4 && (bytes[start] & 0xc0) == 0x80) {
      --start;
    }
    unsigned char lead = (unsigned char)bytes[start];
    size_t width = lead >= 0xf0 && lead <= 0xf4   ? 4
                   : lead >= 0xe0 && lead <= 0xef ? 3
                   : lead >= 0xc2 && lead <= 0xdf ? 2
                                                  : 1;
    if (width > length - start) {
      length = start;
    }
    if (length == 0) {
      return 1;
    }
  }

  for (size_t i = 0; i < length; ++i) {
    if (bytes[i] == '\0') {
      bytes[i] = (char)0xff;
    }
  }
  int wide_length = MultiByteToWideChar(CP_UTF8, 0, bytes, (int)length, NULL, 0);
  if (wide_length == 0) {
    return 0;
  }
  wchar_t *wide = malloc((size_t)wide_length * sizeof(*wide));
  if (wide == NULL) {
    return 0;
  }

  int ok = 0;
  if (MultiByteToWideChar(CP_UTF8, 0, bytes, (int)length, wide, wide_length)) {
    int size = WideCharToMultiByte(CP_UTF8, 0, wide, wide_length, NULL, 0, NULL, NULL);
    char *utf8 = size > 0 ? malloc((size_t)size + 1) : NULL;
    if (utf8 != NULL &&
        WideCharToMultiByte(CP_UTF8, 0, wide, wide_length, utf8, size, NULL, NULL)) {
      if (size > LOG_LIMIT) {
        size = LOG_LIMIT;
        while ((utf8[size] & 0xc0) == 0x80) {
          --size;
        }
        result->logs_truncated = 1;
      }
      utf8[size] = '\0';
      free(result->diagnostics);
      result->diagnostics = utf8;
      ok = 1;
    } else {
      free(utf8);
    }
  }
  free(wide);
  return ok;
}

static BOOL read_log(HANDLE pipe, char *log, size_t *length, int *truncated, BOOL *read_data) {
  DWORD available;
  *read_data = FALSE;
  if (!PeekNamedPipe(pipe, NULL, 0, NULL, &available, NULL)) {
    return GetLastError() == ERROR_BROKEN_PIPE;
  }
  if (available == 0) {
    return TRUE;
  }

  char buffer[8192];
  DWORD wanted = available < sizeof(buffer) ? available : sizeof(buffer);
  DWORD read;
  if (!ReadFile(pipe, buffer, wanted, &read, NULL)) {
    return GetLastError() == ERROR_BROKEN_PIPE;
  }
  size_t remaining = LOG_LIMIT - *length;
  size_t saved = read < remaining ? read : remaining;
  memcpy(log + *length, buffer, saved);
  *length += saved;
  *truncated |= saved < read;
  *read_data = read != 0;
  return TRUE;
}

static DWORD wait_for_compiler(HANDLE process, HANDLE cancel_event, HANDLE pipe,
                               unsigned int timeout_ms, char *log, size_t *length, int *truncated) {
  HANDLE handles[] = {process, cancel_event};
  DWORD count = cancel_event != NULL ? 2 : 1;
  ULONGLONG start = GetTickCount64();

  for (;;) {
    DWORD wait = WaitForMultipleObjects(count, handles, FALSE, 0);
    if (wait != WAIT_TIMEOUT) {
      return wait;
    }
    ULONGLONG elapsed = GetTickCount64() - start;
    if (elapsed >= timeout_ms) {
      return WAIT_TIMEOUT;
    }

    BOOL read_data;
    if (!read_log(pipe, log, length, truncated, &read_data)) {
      return WAIT_FAILED;
    }
    if (!read_data) {
      DWORD remaining = (DWORD)(timeout_ms - elapsed);
      wait = WaitForMultipleObjects(count, handles, FALSE,
                                    remaining < WAIT_SLICE_MS ? remaining : WAIT_SLICE_MS);
      if (wait != WAIT_TIMEOUT) {
        return wait;
      }
    }
  }
}

static BOOL stop_job(HANDLE job) {
  if (!TerminateJobObject(job, 1)) {
    return FALSE;
  }
  for (;;) {
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info;
    if (!QueryInformationJobObject(job, JobObjectBasicAccountingInformation, &info, sizeof(info),
                                   NULL)) {
      return FALSE;
    }
    if (info.ActiveProcesses == 0) {
      return TRUE;
    }
    Sleep(1);
  }
}

static wchar_t *compiler_environment(const wchar_t *scratch) {
  wchar_t *original = GetEnvironmentStringsW();
  if (original == NULL) {
    return NULL;
  }
  const wchar_t *end = original;
  while (*end != L'\0') {
    end += wcslen(end) + 1;
  }
  static const wchar_t *names[] = {L"TEMP", L"TMP"};
  size_t capacity = (end - original) + 2 * wcslen(scratch) + 12;
  wchar_t *environment = malloc(capacity * sizeof(*environment));
  if (environment == NULL) {
    FreeEnvironmentStringsW(original);
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    return NULL;
  }

  wchar_t *out = environment;
  const wchar_t *entry = original;
  size_t next = 0;
  while (*entry != L'\0' || next < _countof(names)) {
    size_t length = wcslen(entry);
    const wchar_t *equals = length != 0 ? wcschr(entry + 1, L'=') : NULL;
    int order = CSTR_GREATER_THAN;
    if (length != 0) {
      int name_length = (int)(equals != NULL ? equals - entry : length);
      order = next < _countof(names)
                  ? CompareStringOrdinal(entry, name_length, names[next], -1, TRUE)
                  : CSTR_LESS_THAN;
    }
    if (order == CSTR_LESS_THAN) {
      wmemcpy(out, entry, length + 1);
      out += length + 1;
    } else {
      const wchar_t *name = names[next++];
      size_t name_length = wcslen(name);
      wmemcpy(out, name, name_length);
      out += name_length;
      *out++ = L'=';
      size_t scratch_length = wcslen(scratch);
      wmemcpy(out, scratch, scratch_length + 1);
      out += scratch_length + 1;
    }
    if (length != 0 && order != CSTR_GREATER_THAN) {
      entry += length + 1;
    }
  }
  *out = L'\0';
  FreeEnvironmentStringsW(original);
  return environment;
}

static void run_compiler(CompileResult *result, const wchar_t *compiler_path, wchar_t *command_line,
                         const wchar_t *work_dir, const wchar_t *scratch, unsigned int timeout_ms,
                         HANDLE cancel_event) {
  SECURITY_ATTRIBUTES inherit = {sizeof(inherit), NULL, TRUE};
  HANDLE reader = NULL;
  HANDLE writer = NULL;
  HANDLE input = INVALID_HANDLE_VALUE;
  HANDLE job = NULL;
  PROCESS_INFORMATION process = {0};
  LPPROC_THREAD_ATTRIBUTE_LIST attributes = NULL;
  BOOL attributes_initialized = FALSE;
  DWORD error = 0;
  char *log = malloc(LOG_LIMIT);
  wchar_t *environment = NULL;
  size_t length = 0;

  if (log == NULL) {
    return;
  }
  environment = compiler_environment(scratch);
  if (environment == NULL) {
    error = GetLastError();
    goto cleanup;
  }
  if (!CreatePipe(&reader, &writer, &inherit, 0) ||
      !SetHandleInformation(reader, HANDLE_FLAG_INHERIT, 0)) {
    error = GetLastError();
    goto cleanup;
  }
  input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (input == INVALID_HANDLE_VALUE) {
    error = GetLastError();
    goto cleanup;
  }

  SIZE_T attribute_size = 0;
  InitializeProcThreadAttributeList(NULL, 2, 0, &attribute_size);
  attributes = malloc(attribute_size);
  if (attributes == NULL) {
    goto cleanup;
  }
  if (!InitializeProcThreadAttributeList(attributes, 2, 0, &attribute_size)) {
    error = GetLastError();
    goto cleanup;
  }
  attributes_initialized = TRUE;
  HANDLE inherited[] = {input, writer};
  if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited,
                                 sizeof(inherited), NULL, NULL)) {
    error = GetLastError();
    goto cleanup;
  }
  job = create_compiler_job();
  if (job == NULL || !UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST, &job,
                                                sizeof(job), NULL, NULL)) {
    error = GetLastError();
    goto cleanup;
  }

  STARTUPINFOEXW startup = {0};
  startup.StartupInfo.cb = sizeof(startup);
  startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  startup.StartupInfo.hStdInput = input;
  startup.StartupInfo.hStdOutput = writer;
  startup.StartupInfo.hStdError = writer;
  startup.lpAttributeList = attributes;
  if (!CreateProcessW(compiler_path, command_line, NULL, NULL, TRUE,
                      CREATE_SUSPENDED | CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT |
                          EXTENDED_STARTUPINFO_PRESENT,
                      environment, work_dir, &startup.StartupInfo, &process)) {
    error = GetLastError();
    result->status = COMPILE_LAUNCH_ERROR;
    goto cleanup;
  }
  CloseHandle(writer);
  writer = NULL;

  if (ResumeThread(process.hThread) == (DWORD)-1) {
    error = GetLastError();
    goto cleanup;
  }

  DWORD wait = wait_for_compiler(process.hProcess, cancel_event, reader, timeout_ms, log, &length,
                                 &result->logs_truncated);
  if (wait == WAIT_OBJECT_0) {
    if (GetExitCodeProcess(process.hProcess, &result->exit_code)) {
      result->has_exit_code = 1;
      result->status = result->exit_code == 0 ? COMPILE_SUCCESS : COMPILE_ERROR;
    } else {
      error = GetLastError();
    }
  } else if (wait == WAIT_OBJECT_0 + 1) {
    result->status = COMPILE_CANCELLED;
  } else if (wait == WAIT_TIMEOUT) {
    result->status = COMPILE_TIMEOUT;
  } else {
    error = GetLastError();
  }

cleanup:
  if (process.hProcess != NULL) {
    if (!stop_job(job)) {
      result->cleanup_failed = 1;
    }
  }
  if (job != NULL) {
    CloseHandle(job);
  }
  if (process.hThread != NULL) {
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
  }
  if (attributes_initialized) {
    DeleteProcThreadAttributeList(attributes);
  }
  free(attributes);
  if (input != INVALID_HANDLE_VALUE) {
    CloseHandle(input);
  }
  if (writer != NULL) {
    CloseHandle(writer);
  }
  if (reader != NULL) {
    BOOL read_data;
    do {
      if (!read_log(reader, log, &length, &result->logs_truncated, &read_data)) {
        if (error == 0) {
          error = GetLastError();
        }
        break;
      }
    } while (read_data && !result->cleanup_failed);
    CloseHandle(reader);
  }
  if (error != 0) {
    set_system_error(result, error);
    if (result->status == COMPILE_SUCCESS) {
      result->status = COMPILE_INTERNAL_ERROR;
    }
  } else if (!set_diagnostics(result, log, length)) {
    result->status = COMPILE_INTERNAL_ERROR;
  }
  free(log);
  free(environment);
}

CompileResult compile_source(const char *source, const CompileOptions *options) {
  return compile_source_with_cancel(source, options, NULL);
}

CompileResult compile_source_with_cancel(const char *source, const CompileOptions *options,
                                         void *cancel_event) {
  CompileResult result = {0};
  result.status = COMPILE_INTERNAL_ERROR;
  result.diagnostics = _strdup("");
  if (source == NULL) {
    result.status = COMPILE_INVALID_ARGUMENT;
    return result;
  }
  if (result.diagnostics == NULL) {
    return result;
  }
  if (cancel_event != NULL) {
    DWORD wait = WaitForSingleObject(cancel_event, 0);
    if (wait == WAIT_OBJECT_0) {
      result.status = COMPILE_CANCELLED;
      return result;
    }
    if (wait != WAIT_TIMEOUT) {
      result.status = COMPILE_INVALID_ARGUMENT;
      set_system_error(&result, GetLastError());
      return result;
    }
  }

  const wchar_t *compiler_path = options != NULL ? options->compiler_path : NULL;
  wchar_t bundled_path[MAX_PATH + 1];
  if (compiler_path == NULL) {
    wchar_t exe_path[MAX_PATH + 1];
    DWORD length = GetModuleFileNameW(NULL, exe_path, _countof(exe_path));
    if (length == 0 || length >= _countof(exe_path)) {
      set_system_error(&result, GetLastError());
      return result;
    }
    wchar_t *separator = wcsrchr(exe_path, L'\\');
    if (separator == NULL) {
      return result;
    }
    *separator = L'\0';
    if (FAILED(StringCchPrintfW(bundled_path, _countof(bundled_path),
                                L"%ls\\compiler\\bin\\clang.exe", exe_path))) {
      return result;
    }
    compiler_path = bundled_path;
  }

  CompileWorkspace *workspace = compile_workspace_create();
  if (workspace == NULL) {
    set_system_error(&result, GetLastError());
    return result;
  }
  const wchar_t *work_dir = compile_workspace_dir(workspace);
  const wchar_t *source_path = compile_workspace_source(workspace);
  const wchar_t *output_path = compile_workspace_output(workspace);

  wchar_t command_line[4 * (MAX_PATH + 1)];
  if (FAILED(StringCchPrintfW(command_line, _countof(command_line),
                              L"\"%ls\" -std=c23 -Wall -Wextra \"%ls\" -o \"%ls\"", compiler_path,
                              source_path, output_path))) {
    goto cleanup;
  }

  HANDLE file =
      CreateFileW(source_path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == INVALID_HANDLE_VALUE) {
    set_system_error(&result, GetLastError());
    goto cleanup;
  }
  size_t source_length = strlen(source);
  DWORD written = 0;
  BOOL write_ok =
      source_length <= MAXDWORD && WriteFile(file, source, (DWORD)source_length, &written, NULL);
  DWORD write_error = source_length > MAXDWORD ? ERROR_FILE_TOO_LARGE
                      : write_ok               ? 0
                                               : GetLastError();
  BOOL closed = CloseHandle(file);
  if (!write_ok || written != source_length || !closed) {
    if (write_error != 0 || !closed) {
      set_system_error(&result, write_error != 0 ? write_error : GetLastError());
    }
    goto cleanup;
  }

  unsigned int timeout_ms =
      options != NULL && options->timeout_ms != 0 ? options->timeout_ms : 10000u;
  run_compiler(&result, compiler_path, command_line, work_dir, compile_workspace_scratch(workspace),
               timeout_ms, cancel_event);
  if (result.status == COMPILE_SUCCESS) {
    result.status = COMPILE_INTERNAL_ERROR;
    if (result.cleanup_failed) {
      goto cleanup;
    }
    HANDLE output = CreateFileW(output_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (output != INVALID_HANDLE_VALUE) {
      BY_HANDLE_FILE_INFORMATION info;
      if (GetFileInformationByHandle(output, &info) &&
          !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
        result.output_path = _wcsdup(output_path);
        if (result.output_path != NULL) {
          result.status = COMPILE_SUCCESS;
        }
      }
      CloseHandle(output);
    } else {
      set_system_error(&result, GetLastError());
    }
  }

cleanup:
  if (result.cleanup_failed) {
    compile_workspace_defer(workspace);
  } else if (!compile_workspace_finish(workspace, result.status == COMPILE_SUCCESS)) {
    result.cleanup_failed = 1;
  }
  return result;
}
