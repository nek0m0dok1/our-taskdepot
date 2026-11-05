#include <stdlib.h>
#include <wchar.h>

#include "compiler/compiler.h"
#include "unity.h"

static CompileResult result;

void setUp(void) {
  result = (CompileResult){0};
}

void tearDown(void) {
  // Assertions that return early still leave allocations for this cleanup.
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
  return UNITY_END();
}
