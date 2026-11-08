
#include "matcher.h"
#include "unity.h"

#include <stddef.h>
#include <string.h>

/* 各テストの実行前に呼ばれる */
void setUp(void) {
}

/* 各テストの実行後に呼ばれる */
void tearDown(void) {
}

/* 登録済みのパターンに一致するか確認 */
void test_pattern_found(void) {
  ErrorPattern patterns[] = {{"missing-semicolon", "expected ';'", "セミコロンが必要です",
                              "直前の文末を確認してください", 10}};

  MatchResult result = match_error("expected ';' before 'return'", patterns, 1);

  TEST_ASSERT_EQUAL(MATCH_FOUND, result.status);
  TEST_ASSERT_NOT_NULL(result.matched_pattern);

  TEST_ASSERT_EQUAL_STRING("missing-semicolon", result.matched_pattern->id);

  TEST_ASSERT_EQUAL_STRING("セミコロンが必要です", result.matched_pattern->explanation);

  TEST_ASSERT_EQUAL_STRING("直前の文末を確認してください", result.matched_pattern->cause);
}

/* 未登録のエラーが正しく判定されるか確認 */
void test_unsupported_error(void) {
  ErrorPattern patterns[] = {{"missing-semicolon", "expected ';'", "説明", "原因", 10}};

  const char *message = "unknown diagnostic";

  MatchResult result = match_error(message, patterns, 1);

  TEST_ASSERT_EQUAL(MATCH_UNSUPPORTED, result.status);
  TEST_ASSERT_NULL(result.matched_pattern);

  TEST_ASSERT_EQUAL_STRING(message, result.original_message);
}

/* NULLが渡された場合の確認 */
void test_null_input(void) {
  MatchResult result = match_error(NULL, NULL, 0);

  TEST_ASSERT_EQUAL(MATCH_INVALID_INPUT, result.status);
}

/* 空文字列が渡された場合の確認 */
void test_empty_input(void) {
  MatchResult result = match_error("", NULL, 0);

  TEST_ASSERT_EQUAL(MATCH_INVALID_INPUT, result.status);
}

/* 複数一致した場合の優先順位を確認 */
void test_priority(void) {
  ErrorPattern patterns[] = {{"general", "expected", "一般的なエラー", "原因", 1},
                             {"specific", "expected ';'", "セミコロン不足", "原因", 10}};

  MatchResult result = match_error("expected ';' before 'return'", patterns, 2);

  TEST_ASSERT_EQUAL(MATCH_FOUND, result.status);

  TEST_ASSERT_EQUAL_STRING("specific", result.matched_pattern->id);
}

/* 優先順位が同じ場合は長いパターンを選ぶ */
void test_longer_pattern(void) {
  ErrorPattern patterns[] = {{"short", "expected", "説明", "原因", 10},
                             {"long", "expected ';'", "説明", "原因", 10}};

  MatchResult result = match_error("expected ';' before 'return'", patterns, 2);

  TEST_ASSERT_EQUAL(MATCH_FOUND, result.status);
  TEST_ASSERT_EQUAL_STRING("long", result.matched_pattern->id);
}

/* パターンが0件の場合は未対応になる */
void test_no_patterns(void) {
  MatchResult result = match_error("unknown error", NULL, 0);

  TEST_ASSERT_EQUAL(MATCH_UNSUPPORTED, result.status);
}

/* パターン配列が不正な場合 */
void test_invalid_pattern_array(void) {
  MatchResult result = match_error("expected ';'", NULL, 1);

  TEST_ASSERT_EQUAL(MATCH_INVALID_INPUT, result.status);
}

/* テストを順番に実行 */
int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_pattern_found);
  RUN_TEST(test_unsupported_error);
  RUN_TEST(test_null_input);
  RUN_TEST(test_empty_input);
  RUN_TEST(test_priority);
  RUN_TEST(test_longer_pattern);
  RUN_TEST(test_no_patterns);
  RUN_TEST(test_invalid_pattern_array);

  return UNITY_END();
}
