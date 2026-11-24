#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#include <wchar.h>

#include "compiler/compiler.h"
#include "unity.h"

static CompileResult result;
static const wchar_t *fixture;
static const wchar_t *clang_path;

void setUp(void) {
  result = (CompileResult){0};
}

void tearDown(void) {
  if (result.output_path != NULL) {
    compile_output_delete(result.output_path);
  }
  compile_result_free(&result);
}

/*想定：sourceが存在しない
 *期待値：COMPILE_INVARID_ARGUMENT
 */
void test_compile_source_null_source(void) {
  CompileOptions options = {0};

  options.compiler_path = L"C:\\compiler\\clang.exe";
  options.timeout_ms = 5000;

  result = compile_source(NULL, &options);

  TEST_ASSERT_EQUAL_INT(COMPILE_INVALID_ARGUMENT, result.status);
  TEST_ASSERT_EQUAL_STRING("", result.diagnostics);
}

static void assert_success(void) {
  TEST_ASSERT_EQUAL_INT_MESSAGE(COMPILE_SUCCESS, result.status, result.diagnostics);
  TEST_ASSERT_TRUE(result.has_exit_code);
  TEST_ASSERT_EQUAL_UINT32(0, result.exit_code);
  TEST_ASSERT_FALSE(result.cleanup_failed);
  TEST_ASSERT_NOT_NULL(result.output_path);
  TEST_ASSERT_NOT_EQUAL(INVALID_FILE_ATTRIBUTES, GetFileAttributesW(result.output_path));

  wchar_t source[MAX_PATH + 1];
  const wchar_t *separator = wcsrchr(result.output_path, L'\\');
  TEST_ASSERT_NOT_NULL(separator);
  TEST_ASSERT_TRUE(
      SUCCEEDED(StringCchPrintfW(source, _countof(source), L"%.*ls\\source.c",
                                 (int)(separator - result.output_path), result.output_path)));
  TEST_ASSERT_EQUAL_UINT32(INVALID_FILE_ATTRIBUTES, GetFileAttributesW(source));
}

static void test_compile_source_null_options(void) {
  result = compile_source("success", NULL);
  assert_success();
  TEST_ASSERT_EQUAL_STRING("", result.diagnostics);
  TEST_ASSERT_TRUE(compile_output_delete(result.output_path));
  TEST_ASSERT_EQUAL_UINT32(INVALID_FILE_ATTRIBUTES, GetFileAttributesW(result.output_path));
}

static void test_compile_source_null_compiler_path(void) {
  CompileOptions options = {0};
  options.timeout_ms = 5000;
  result = compile_source("", &options);
  TEST_ASSERT_EQUAL_INT(COMPILE_ERROR, result.status);
  TEST_ASSERT_EQUAL_STRING("error: input\n", result.diagnostics);
  TEST_ASSERT_NULL(result.output_path);
  TEST_ASSERT_FALSE(result.cleanup_failed);
}

static void test_compile_launch_error(void) {
  CompileOptions options = {L"C:\\missing our-taskdepot compiler\\clang.exe", 0};
  result = compile_source("success", &options);
  TEST_ASSERT_EQUAL_INT(COMPILE_LAUNCH_ERROR, result.status);
  TEST_ASSERT_FALSE(result.has_exit_code);
  TEST_ASSERT_NOT_EQUAL(0, strlen(result.diagnostics));
  TEST_ASSERT_NULL(result.output_path);
  TEST_ASSERT_FALSE(result.cleanup_failed);
}

static void test_compile_missing_output(void) {
  CompileOptions options = {fixture, 0};
  result = compile_source("no output", &options);
  TEST_ASSERT_EQUAL_INT(COMPILE_INTERNAL_ERROR, result.status);
  TEST_ASSERT_TRUE(result.has_exit_code);
  TEST_ASSERT_EQUAL_UINT32(0, result.exit_code);
  TEST_ASSERT_NULL(result.output_path);
  TEST_ASSERT_FALSE(result.cleanup_failed);
}

static void test_compile_invalid_utf8(void) {
  CompileOptions options = {fixture, 0};
  result = compile_source("invalid log", &options);
  TEST_ASSERT_EQUAL_INT(COMPILE_ERROR, result.status);
  TEST_ASSERT_EQUAL_UINT32(3, result.exit_code);
  TEST_ASSERT_EQUAL_STRING("out\xef\xbf\xbd\xef\xbf\xbd\xe6\x97\xa5", result.diagnostics);
  TEST_ASSERT_FALSE(result.logs_truncated);
}

static void test_compile_large_log(void) {
  CompileOptions options = {fixture, 0};
  result = compile_source("large log", &options);
  TEST_ASSERT_EQUAL_INT(COMPILE_ERROR, result.status);
  TEST_ASSERT_EQUAL_UINT32(3, result.exit_code);
  TEST_ASSERT_TRUE(result.logs_truncated);
  TEST_ASSERT_EQUAL_UINT32(1024u * 1024u - 1u, strlen(result.diagnostics));
  TEST_ASSERT_EQUAL_CHAR('a', result.diagnostics[1024u * 1024u - 2u]);
}

static void test_compile_timeout(void) {
  CompileOptions options = {fixture, 600};
  ULONGLONG start = GetTickCount64();
  result = compile_source("continuous log", &options);
  TEST_ASSERT_EQUAL_INT(COMPILE_TIMEOUT, result.status);
  TEST_ASSERT_TRUE(GetTickCount64() - start < 4000);
  TEST_ASSERT_FALSE(result.has_exit_code);
  TEST_ASSERT_FALSE(result.cleanup_failed);
  TEST_ASSERT_NULL(result.output_path);
}

static void test_compile_output_delete_failure(void) {
  result = compile_source("success", NULL);
  assert_success();
  HANDLE locked = CreateFileW(result.output_path, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  TEST_ASSERT_NOT_EQUAL(INVALID_HANDLE_VALUE, locked);
  int deleted = compile_output_delete(result.output_path);
  BOOL still_present = GetFileAttributesW(result.output_path) != INVALID_FILE_ATTRIBUTES;
  CloseHandle(locked);
  TEST_ASSERT_FALSE(deleted);
  TEST_ASSERT_TRUE(still_present);
  TEST_ASSERT_EQUAL_INT(0, compile_recover_workspaces());
  TEST_ASSERT_EQUAL_UINT32(INVALID_FILE_ATTRIBUTES, GetFileAttributesW(result.output_path));
}

static void test_compile_recovery_after_directory_delete_failure(void) {
  result = compile_source("success", NULL);
  assert_success();
  wchar_t directory[MAX_PATH + 1];
  wchar_t extra_file[MAX_PATH + 1];
  const wchar_t *separator = wcsrchr(result.output_path, L'\\');
  TEST_ASSERT_TRUE(SUCCEEDED(StringCchCopyNW(directory, _countof(directory), result.output_path,
                                             separator - result.output_path)));
  TEST_ASSERT_TRUE(
      SUCCEEDED(StringCchPrintfW(extra_file, _countof(extra_file), L"%ls\\keep.txt", directory)));
  HANDLE extra =
      CreateFileW(extra_file, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
  TEST_ASSERT_NOT_EQUAL(INVALID_HANDLE_VALUE, extra);
  CloseHandle(extra);
  int deleted = compile_output_delete(result.output_path);
  TEST_ASSERT_FALSE(deleted);
  TEST_ASSERT_EQUAL_INT(1, compile_recover_workspaces());
  TEST_ASSERT_NOT_EQUAL(INVALID_FILE_ATTRIBUTES, GetFileAttributesW(extra_file));
  TEST_ASSERT_TRUE(DeleteFileW(extra_file));
  TEST_ASSERT_EQUAL_INT(0, compile_recover_workspaces());
  TEST_ASSERT_EQUAL_UINT32(INVALID_FILE_ATTRIBUTES, GetFileAttributesW(directory));
}

static int leave_output(const wchar_t *record) {
  CompileResult created = compile_source("success", NULL);
  if (created.status != COMPILE_SUCCESS) {
    return EXIT_FAILURE;
  }
  FILE *file;
  if (_wfopen_s(&file, record, L"wb") != 0) {
    return EXIT_FAILURE;
  }
  size_t length = wcslen(created.output_path) + 1;
  return fwrite(created.output_path, sizeof(wchar_t), length, file) == length && fclose(file) == 0
             ? EXIT_SUCCESS
             : EXIT_FAILURE;
}

static void test_compile_recover_abandoned_output(void) {
  wchar_t temp[MAX_PATH + 1];
  wchar_t record[MAX_PATH + 1];
  wchar_t self[MAX_PATH + 1];
  wchar_t command[3 * MAX_PATH];
  TEST_ASSERT_NOT_EQUAL(0, GetTempPath2W(_countof(temp), temp));
  TEST_ASSERT_NOT_EQUAL(0, GetModuleFileNameW(NULL, self, _countof(self)));
  TEST_ASSERT_NOT_EQUAL(0, GetTempFileNameW(temp, L"ct", 0, record));
  TEST_ASSERT_TRUE(SUCCEEDED(StringCchPrintfW(command, _countof(command),
                                              L"\"%ls\" --leave-output \"%ls\"", self, record)));
  STARTUPINFOW startup = {sizeof(startup)};
  PROCESS_INFORMATION process;
  TEST_ASSERT_TRUE(CreateProcessW(self, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL,
                                  &startup, &process));
  DWORD wait = WaitForSingleObject(process.hProcess, 5000);
  DWORD exit_code = EXIT_FAILURE;
  GetExitCodeProcess(process.hProcess, &exit_code);
  CloseHandle(process.hProcess);
  CloseHandle(process.hThread);
  TEST_ASSERT_EQUAL_UINT32(WAIT_OBJECT_0, wait);
  TEST_ASSERT_EQUAL_UINT32(EXIT_SUCCESS, exit_code);

  FILE *file;
  TEST_ASSERT_EQUAL_INT(0, _wfopen_s(&file, record, L"rb"));
  wchar_t output[MAX_PATH + 1] = {0};
  size_t length = fread(output, sizeof(wchar_t), _countof(output), file);
  fclose(file);
  DeleteFileW(record);
  TEST_ASSERT_TRUE(length > 1 && output[length - 1] == L'\0');
  TEST_ASSERT_NOT_EQUAL(INVALID_FILE_ATTRIBUTES, GetFileAttributesW(output));
  TEST_ASSERT_EQUAL_INT(0, compile_recover_workspaces());
  TEST_ASSERT_EQUAL_UINT32(INVALID_FILE_ATTRIBUTES, GetFileAttributesW(output));
}

static void test_real_clang_success(void) {
  CompileOptions options = {clang_path, 0};
  result = compile_source("int main(void) { int unused = 0; return 0; }", &options);
  assert_success();
  TEST_ASSERT_NOT_NULL(strstr(result.diagnostics, "warning:"));
}

static void test_real_clang_utf8_error(void) {
  CompileOptions options = {clang_path, 0};
  result = compile_source("#error 日本語\n", &options);
  TEST_ASSERT_EQUAL_INT_MESSAGE(COMPILE_ERROR, result.status, result.diagnostics);
  TEST_ASSERT_TRUE(result.has_exit_code);
  TEST_ASSERT_NULL(result.output_path);
  TEST_ASSERT_NOT_NULL(strstr(result.diagnostics, "日本語"));
  TEST_ASSERT_FALSE(result.cleanup_failed);
}

static void test_compile_result_free_accepts_null_argument(void) {
  compile_result_free(NULL);
}

static void test_compile_result_free_accepts_both_members_null(void) {
  compile_result_free(&result);
  TEST_ASSERT_NULL(result.diagnostics);
  TEST_ASSERT_NULL(result.output_path);
}

static void test_compile_result_free_clears_diagnostics(void) {
  result.diagnostics = malloc(sizeof(char));
  TEST_ASSERT_NOT_NULL(result.diagnostics);

  compile_result_free(&result);

  TEST_ASSERT_NULL(result.diagnostics);
  TEST_ASSERT_NULL(result.output_path);
}

static void test_compile_result_free_clears_output_path(void) {
  result.output_path = malloc(sizeof(wchar_t));
  TEST_ASSERT_NOT_NULL(result.output_path);

  compile_result_free(&result);

  TEST_ASSERT_NULL(result.diagnostics);
  TEST_ASSERT_NULL(result.output_path);
}

static void test_compile_result_free_clears_both_members(void) {
  result.diagnostics = malloc(sizeof(char));
  result.output_path = malloc(sizeof(wchar_t));
  TEST_ASSERT_NOT_NULL(result.diagnostics);
  TEST_ASSERT_NOT_NULL(result.output_path);

  compile_result_free(&result);

  TEST_ASSERT_NULL(result.diagnostics);
  TEST_ASSERT_NULL(result.output_path);
}

static void test_compile_result_free_can_be_called_repeatedly(void) {
  result.diagnostics = malloc(sizeof(char));
  result.output_path = malloc(sizeof(wchar_t));
  TEST_ASSERT_NOT_NULL(result.diagnostics);
  TEST_ASSERT_NOT_NULL(result.output_path);

  compile_result_free(&result);
  compile_result_free(&result);

  TEST_ASSERT_NULL(result.diagnostics);
  TEST_ASSERT_NULL(result.output_path);
}

int wmain(int argc, wchar_t **argv) {
  if (argc == 3 && wcscmp(argv[1], L"--leave-output") == 0) {
    return leave_output(argv[2]);
  }
  if (argc == 3 && wcscmp(argv[1], L"--llvm-mingw") == 0) {
    clang_path = argv[2];
    UNITY_BEGIN();
    RUN_TEST(test_real_clang_success);
    RUN_TEST(test_real_clang_utf8_error);
    return UNITY_END();
  }
  if (argc != 2) {
    return EXIT_FAILURE;
  }
  fixture = argv[1];
  UNITY_BEGIN();
  RUN_TEST(test_compile_result_free_accepts_null_argument);
  RUN_TEST(test_compile_result_free_accepts_both_members_null);
  RUN_TEST(test_compile_result_free_clears_diagnostics);
  RUN_TEST(test_compile_result_free_clears_output_path);
  RUN_TEST(test_compile_result_free_clears_both_members);
  RUN_TEST(test_compile_result_free_can_be_called_repeatedly);
  RUN_TEST(test_compile_source_null_source);
  RUN_TEST(test_compile_source_null_options);
  RUN_TEST(test_compile_source_null_compiler_path);
  RUN_TEST(test_compile_launch_error);
  RUN_TEST(test_compile_missing_output);
  RUN_TEST(test_compile_invalid_utf8);
  RUN_TEST(test_compile_large_log);
  RUN_TEST(test_compile_timeout);
  RUN_TEST(test_compile_output_delete_failure);
  RUN_TEST(test_compile_recovery_after_directory_delete_failure);
  RUN_TEST(test_compile_recover_abandoned_output);
  return UNITY_END();
}
