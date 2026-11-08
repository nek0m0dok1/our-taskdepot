
#include "mock_backend.h"
#include <string.h>
// Keep the existing GUI function working
int mock_compile(const char *code) {
  if (code == NULL || code[0] == '\0') {
    return 1;
  }

  if (strstr(code, "error") != NULL) {
    return 1;
  }

  return 0;
}
// Mock compile result
MockResult mock_compile_result(const char *code) {
  MockResult r = {MOCK_COMPILE_SUCCESS, "", "", 0, "", "", 0};

  if (code == NULL || code[0] == '\0') {
    r.status = MOCK_COMPILE_ERROR;
    r.japanese_message = "コードが入力されていません。";
    r.original_message = "No source code";
    r.remaining_errors = 1;
    r.exit_status = 1;
  } else if (strstr(code, "compiling") != NULL) {
    r.status = MOCK_COMPILING;
    r.japanese_message = "コンパイル中です。";
  } else if (strstr(code, "multiple") != NULL) {
    r.status = MOCK_MULTIPLE_ERRORS;
    r.japanese_message = "複数のエラーが発生しました。";
    r.original_message = "Multiple compilation errors";
    r.remaining_errors = 2;
    r.stderr_text = "error: multiple errors";
    r.exit_status = 1;
  } else if (strstr(code, "unknown") != NULL) {
    r.status = MOCK_UNKNOWN_ERROR;
    r.japanese_message = "未対応のエラーです。";
    r.original_message = "Unknown compiler error";
    r.remaining_errors = 1;
    r.stderr_text = "error: unknown";
    r.exit_status = 1;
  } else if (strstr(code, "error") != NULL) {
    r.status = MOCK_COMPILE_ERROR;
    r.japanese_message = "コンパイルエラーが発生しました。";
    r.original_message = "Compilation error";
    r.remaining_errors = 1;
    r.stderr_text = "error: compilation failed";
    r.exit_status = 1;
  } else {
    r.japanese_message = "コンパイルに成功しました。";
  }

  return r;
}
// Mock run result
MockResult mock_run_result(const char *code) {
  MockResult r = {
      MOCK_RUN_SUCCESS, "プログラムの実行に成功しました。", "", 0, "Hello World\n", "", 0};

  if (code == NULL || strstr(code, "run_error") != NULL) {
    r.status = MOCK_RUN_ERROR;
    r.japanese_message = "プログラムの実行に失敗しました。";
    r.original_message = "Runtime error";
    r.stdout_text = "";
    r.stderr_text = "Runtime error occurred";
    r.exit_status = 1;
  } else {
    r.status = MOCK_RUN_SUCCESS;
    r.japanese_message = "プログラムの実行に成功しました。";
    r.original_message = "Run successful";
    r.stdout_text = "Hello World";
    r.stderr_text = "";
    r.exit_status = 0;
  }
  return r;
}
