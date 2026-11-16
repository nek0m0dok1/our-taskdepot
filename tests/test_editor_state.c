#include "editor_state.h"
#include "unity.h"

#include <stddef.h>
#include <wchar.h>

/* 各テストの実行前に呼ばれる */
void setUp(void) {
}

/* 各テストの実行後に呼ばれる */
void tearDown(void) {
}

/* 初期状態を確認する */
void test_initial_state(void) {
  EditorState state;

  editor_state_init(&state);

  TEST_ASSERT_NULL(state.current_path);
  TEST_ASSERT_FALSE(state.is_modified);

  editor_state_free(&state);
}

/* 編集されたことを記録できるか確認する */
void test_mark_modified(void) {
  EditorState state;

  editor_state_init(&state);
  editor_state_mark_modified(&state);

  TEST_ASSERT_TRUE(state.is_modified);

  editor_state_free(&state);
}

/* 未保存の変更があるか確認する */
void test_has_unsaved_changes(void) {
  EditorState state;

  editor_state_init(&state);

  TEST_ASSERT_FALSE(editor_state_has_unsaved_changes(&state));

  editor_state_mark_modified(&state);

  TEST_ASSERT_TRUE(editor_state_has_unsaved_changes(&state));

  editor_state_free(&state);
}

/* メモリ解放後の状態を確認する */
void test_free_state(void) {
  EditorState state;

  editor_state_init(&state);
  editor_state_mark_modified(&state);

  editor_state_free(&state);

  TEST_ASSERT_NULL(state.current_path);
  TEST_ASSERT_FALSE(state.is_modified);
}

/* NULLを渡しても異常終了しないか確認する */
void test_null_argument(void) {
  editor_state_init(NULL);
  editor_state_mark_modified(NULL);
  editor_state_free(NULL);

  TEST_ASSERT_FALSE(editor_state_has_unsaved_changes(NULL));
}

/* ファイルパスを登録できるか確認する */
void test_set_path(void) {
  EditorState state;
  editor_state_init(&state);

  TEST_ASSERT_TRUE(editor_state_set_path(&state, L"sample.c"));
  TEST_ASSERT_NOT_NULL(state.current_path);
  TEST_ASSERT_EQUAL_INT(0, wcscmp(L"sample.c", state.current_path));

  editor_state_free(&state);
}

/* 日本語を含むファイルパスを登録できるか確認する */
void test_japanese_path(void) {
  EditorState state;
  editor_state_init(&state);

  const wchar_t *path = L"C:\\Users\\test\\日本語 file.c";

  TEST_ASSERT_TRUE(editor_state_set_path(&state, path));
  TEST_ASSERT_EQUAL_INT(0, wcscmp(path, state.current_path));

  editor_state_free(&state);
}

/* 登録済みのファイルパスを変更できるか確認する */
void test_replace_path(void) {
  EditorState state;
  editor_state_init(&state);

  TEST_ASSERT_TRUE(editor_state_set_path(&state, L"first.c"));
  TEST_ASSERT_TRUE(editor_state_set_path(&state, L"second.c"));

  TEST_ASSERT_EQUAL_INT(0, wcscmp(L"second.c", state.current_path));

  editor_state_free(&state);
}

/* 不正な引数を拒否できるか確認する */
void test_set_path_invalid_argument(void) {
  EditorState state;
  editor_state_init(&state);

  TEST_ASSERT_FALSE(editor_state_set_path(NULL, L"sample.c"));
  TEST_ASSERT_FALSE(editor_state_set_path(&state, NULL));
  TEST_ASSERT_NULL(state.current_path);

  editor_state_free(&state);
}

/* パス登録後も未保存の編集状態が保持されるか確認する */
void test_set_path_preserves_modified(void) {
  EditorState state;
  editor_state_init(&state);

  editor_state_mark_modified(&state);

  TEST_ASSERT_TRUE(editor_state_set_path(&state, L"sample.c"));

  TEST_ASSERT_TRUE(editor_state_has_unsaved_changes(&state));

  editor_state_free(&state);
}

/* 未保存の変更がある場合に確認が必要か調べる */
void test_requires_confirmation(void) {
  EditorState state;

  editor_state_init(&state);

  /* 初期状態では確認不要 */
  TEST_ASSERT_FALSE(editor_state_requires_confirmation(&state));

  /* 編集すると確認が必要になる */
  editor_state_mark_modified(&state);

  TEST_ASSERT_TRUE(editor_state_requires_confirmation(&state));

  editor_state_free(&state);

  /* NULLでも異常終了しない */
  TEST_ASSERT_FALSE(editor_state_requires_confirmation(NULL));
}

/* テストを実行する */
int main(void) {
  UNITY_BEGIN();

  /* 既存のテスト */
  RUN_TEST(test_initial_state);
  RUN_TEST(test_mark_modified);
  RUN_TEST(test_has_unsaved_changes);
  RUN_TEST(test_free_state);
  RUN_TEST(test_null_argument);

  /* ファイルパス管理のテスト */
  RUN_TEST(test_set_path);
  RUN_TEST(test_japanese_path);
  RUN_TEST(test_replace_path);
  RUN_TEST(test_set_path_invalid_argument);
  RUN_TEST(test_set_path_preserves_modified);
  RUN_TEST(test_requires_confirmation);

  return UNITY_END();
}
