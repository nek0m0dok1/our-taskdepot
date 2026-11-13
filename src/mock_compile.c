
#include "mock_backend.h"
#include <string.h>

/* Japanese messages encoded as UTF-8 hex bytes */

#define JP_NO_CODE                                                                                 \
  "\xE3\x82\xB3\xE3\x83\xBC\xE3\x83\x89\xE3\x81\x8C\xE5\x85\xA5\xE5\x8A\x9B\xE3\x81\x95\xE3\x82"   \
  "\x8C\xE3\x81\xA6\xE3\x81\x84\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93\xE3\x80\x82"

#define JP_COMPILING                                                                               \
  "\xE3\x82\xB3\xE3\x83\xB3\xE3\x83\x91\xE3\x82\xA4\xE3\x83\xAB\xE4\xB8\xAD\xE3\x81\xA7\xE3\x81"   \
  "\x99\xE3\x80\x82"

#define JP_MULTIPLE                                                                                \
  "\xE8\xA4\x87\xE6\x95\xB0\xE3\x81\xAE\xE3\x82\xA8\xE3\x83\xA9\xE3\x83\xBC\xE3\x81\x8C\xE7\x99"   \
  "\xBA\xE7\x94\x9F\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x97\xE3\x81\x9F\xE3\x80\x82"

#define JP_UNKNOWN                                                                                 \
  "\xE6\x9C\xAA\xE5\xAF\xBE\xE5\xBF\x9C\xE3\x81\xAE\xE3\x82\xA8\xE3\x83\xA9\xE3\x83\xBC\xE3\x81"   \
  "\xA7\xE3\x81\x99\xE3\x80\x82"

#define JP_COMPILE_ERROR                                                                           \
  "\xE3\x82\xB3\xE3\x83\xB3\xE3\x83\x91\xE3\x82\xA4\xE3\x83\xAB\xE3\x82\xA8\xE3\x83\xA9\xE3\x83"   \
  "\xBC\xE3\x81\x8C\xE7\x99\xBA\xE7\x94\x9F\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x97\xE3\x81\x9F\xE3"   \
  "\x80\x82"

#define JP_COMPILE_SUCCESS                                                                         \
  "\xE3\x82\xB3\xE3\x83\xB3\xE3\x83\x91\xE3\x82\xA4\xE3\x83\xAB\xE3\x81\xAB\xE6\x88\x90\xE5\x8A"   \
  "\x9F\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x97\xE3\x81\x9F\xE3\x80\x82"

#define JP_RUN_SUCCESS                                                                             \
  "\xE3\x83\x97\xE3\x83\xAD\xE3\x82\xB0\xE3\x83\xA9\xE3\x83\xA0\xE3\x81\xAE\xE5\xAE\x9F\xE8\xA1"   \
  "\x8C\xE3\x81\xAB\xE6\x88\x90\xE5\x8A\x9F\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x97\xE3\x81\x9F\xE3"   \
  "\x80\x82"

#define JP_RUN_ERROR                                                                               \
  "\xE3\x83\x97\xE3\x83\xAD\xE3\x82\xB0\xE3\x83\xA9\xE3\x83\xA0\xE3\x81\xAE\xE5\xAE\x9F\xE8\xA1"   \
  "\x8C\xE3\x81\xAB\xE5\xA4\xB1\xE6\x95\x97\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x97\xE3\x81\x9F\xE3"   \
  "\x80\x82"

/* Existing compile function */

int mock_compile(const char *code) {
  if (code == NULL || code[0] == '\0') {
    return 1;
  }

  if (strstr(code, "error") != NULL) {
    return 1;
  }

  if (strstr(code, "multiple") != NULL || strstr(code, "unknown") != NULL) {
    return 1;
  }

  return 0;
}

/* Mock compile result */

MockResult mock_compile_result(const char *code) {
  MockResult r = {MOCK_COMPILE_SUCCESS, "", "", 0, "", "", 0};

  if (code == NULL || code[0] == '\0') {

    r.status = MOCK_COMPILE_ERROR;
    r.japanese_message = JP_NO_CODE;
    r.original_message = "No source code";
    r.remaining_errors = 1;
    r.exit_status = 1;

  } else if (strstr(code, "compiling") != NULL) {

    r.status = MOCK_COMPILING;
    r.japanese_message = JP_COMPILING;

  } else if (strstr(code, "multiple") != NULL) {

    r.status = MOCK_MULTIPLE_ERRORS;
    r.japanese_message = JP_MULTIPLE;
    r.original_message = "Multiple compilation errors";
    r.remaining_errors = 2;
    r.stderr_text = "error: multiple errors";
    r.exit_status = 1;

  } else if (strstr(code, "unknown") != NULL) {

    r.status = MOCK_UNKNOWN_ERROR;
    r.japanese_message = JP_UNKNOWN;
    r.original_message = "Unknown compiler error";
    r.remaining_errors = 1;
    r.stderr_text = "error: unknown";
    r.exit_status = 1;

  } else if (strstr(code, "error") != NULL) {

    r.status = MOCK_COMPILE_ERROR;
    r.japanese_message = JP_COMPILE_ERROR;
    r.original_message = "Compilation error";
    r.remaining_errors = 1;
    r.stderr_text = "error: compilation failed";
    r.exit_status = 1;

  } else {

    r.status = MOCK_COMPILE_SUCCESS;
    r.japanese_message = JP_COMPILE_SUCCESS;
    r.exit_status = 0;
  }

  return r;
}

/* Mock run result */

MockResult mock_run_result(const char *code) {
  MockResult r = {MOCK_RUN_SUCCESS, JP_RUN_SUCCESS, "", 0, "Hello World\n", "", 0};

  if (code == NULL || strstr(code, "run_error") != NULL) {

    r.status = MOCK_RUN_ERROR;
    r.japanese_message = JP_RUN_ERROR;
    r.original_message = "Runtime error";
    r.stdout_text = "";
    r.stderr_text = "Runtime error occurred";
    r.exit_status = 1;

  } else {

    r.status = MOCK_RUN_SUCCESS;
    r.japanese_message = JP_RUN_SUCCESS;
    r.original_message = "Run successful";
    r.stdout_text = "Hello World";
    r.stderr_text = "";
    r.exit_status = 0;
  }

  return r;
}
