#include "compiler/compiler.h"

#include "compiler_diagnostics.h"
#include "compiler_process.h"
#include "workspace.h"

#include <Windows.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#include <wchar.h>

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
      compiler_set_system_error(&result, GetLastError());
      return result;
    }
  }

  const wchar_t *compiler_path = options != NULL ? options->compiler_path : NULL;
  wchar_t bundled_path[MAX_PATH + 1];
  if (compiler_path == NULL) {
    wchar_t exe_path[MAX_PATH + 1];
    DWORD length = GetModuleFileNameW(NULL, exe_path, _countof(exe_path));
    if (length == 0 || length >= _countof(exe_path)) {
      compiler_set_system_error(&result, GetLastError());
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
    compiler_set_system_error(&result, GetLastError());
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
    compiler_set_system_error(&result, GetLastError());
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
      compiler_set_system_error(&result, write_error != 0 ? write_error : GetLastError());
    }
    goto cleanup;
  }

  unsigned int timeout_ms =
      options != NULL && options->timeout_ms != 0 ? options->timeout_ms : 10000u;
  compiler_run_process(&result, compiler_path, command_line, work_dir,
                       compile_workspace_scratch(workspace), timeout_ms, cancel_event);
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
      compiler_set_system_error(&result, GetLastError());
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
