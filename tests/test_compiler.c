#include <stdlib.h>
#include <wchar.h>

#include "compiler/compiler.h"
#include "unity.h"
#include "unity_internals.h"

static CompileResult result;

void setUp(void) {
  result = (CompileResult){0};
}

void tearDown(void) {
  compile_result_free(&result);
}

/*想定：sourceが存在しない
 *期待値：COMPILE_INVARID_ARGUMENT
 */
void test_compile_source_null_source(void) {
  CompileOptions options = {0};

  options.compiler_path = L"C:\\compiler\\clang.exe";
  options.timeout_ms = 5000;

  CompileResult result = compile_source(NULL, &options);

  TEST_ASSERT_EQUAL_INT(COMPILE_INVALID_ARGUMENT, result.status);

  compile_result_free(&result);
}

/*想定：optionが存在しない
 *期待値：COMPILE_INVARID_ARGUMENT
 */
void test_compile_source_null_options(void) {
  CompileResult result = compile_source("int main(void) { return 0; }", NULL);

  TEST_ASSERT_EQUAL_INT(COMPILE_INVALID_ARGUMENT, result.status);

  compile_result_free(&result);
}

/*想定：compiler_pathが存在しない
 *期待値：COMPILE_INVARID_ARGUMENT
 */
void test_compile_source_null_compiler_path(void) {
  CompileOptions options = {0};

  options.timeout_ms = 5000;

  CompileResult result = compile_source("int main(void) { return 0; }", &options);

  TEST_ASSERT_EQUAL_INT(COMPILE_INVALID_ARGUMENT, result.status);

  compile_result_free(&result);
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

int main(void) {
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
  return UNITY_END();
}
