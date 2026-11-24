#include "compiler_process.h"

#include "compiler_diagnostics.h"

#include <stdlib.h>
#include <wchar.h>

#define WAIT_SLICE_MS 10u

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
    if (!compiler_read_log(pipe, log, length, truncated, &read_data)) {
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

void compiler_run_process(CompileResult *result, const wchar_t *compiler_path,
                          wchar_t *command_line, const wchar_t *work_dir, const wchar_t *scratch,
                          unsigned int timeout_ms, HANDLE cancel_event) {
  SECURITY_ATTRIBUTES inherit = {sizeof(inherit), NULL, TRUE};
  HANDLE reader = NULL;
  HANDLE writer = NULL;
  HANDLE input = INVALID_HANDLE_VALUE;
  HANDLE job = NULL;
  PROCESS_INFORMATION process = {0};
  LPPROC_THREAD_ATTRIBUTE_LIST attributes = NULL;
  BOOL attributes_initialized = FALSE;
  DWORD error = 0;
  char *log = malloc(COMPILER_LOG_LIMIT);
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
      if (!compiler_read_log(reader, log, &length, &result->logs_truncated, &read_data)) {
        if (error == 0) {
          error = GetLastError();
        }
        break;
      }
    } while (read_data && !result->cleanup_failed);
    CloseHandle(reader);
  }
  if (error != 0) {
    compiler_set_system_error(result, error);
    if (result->status == COMPILE_SUCCESS) {
      result->status = COMPILE_INTERNAL_ERROR;
    }
  } else if (!compiler_set_diagnostics(result, log, length)) {
    result->status = COMPILE_INTERNAL_ERROR;
  }
  free(log);
  free(environment);
}
