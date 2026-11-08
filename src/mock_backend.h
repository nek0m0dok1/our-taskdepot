#ifndef MOCK_BACKEND_H
#define MOCK_BACKEND_H
// Compile status
typedef enum {
  MOCK_COMPILING,
  MOCK_COMPILE_SUCCESS,
  MOCK_COMPILE_ERROR,
  MOCK_MULTIPLE_ERRORS,
  MOCK_UNKNOWN_ERROR,
  MOCK_RUN_SUCCESS,
  MOCK_RUN_ERROR
} MockStatus;
// Compile / Run result
typedef struct {
  MockStatus status;

  const char *japanese_message;
  const char *original_message;

  int remaining_errors;

  const char *stdout_text;
  const char *stderr_text;

  int exit_status;
} MockResult;
// Mock backend functions
MockResult mock_compile_result(const char *code);
MockResult mock_run_result(const char *code);
#endif
