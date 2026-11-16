#include "editor_state.h"

#include <stdint.h>
#include <stdlib.h>

/* エディタの状態を初期化する */
void editor_state_init(EditorState *state) {
  if (state == NULL) {
    return;
  }

  state->current_path = NULL;
  state->is_modified = false;
}

/* 編集されたことを記録する */
void editor_state_mark_modified(EditorState *state) {
  if (state == NULL) {
    return;
  }

  state->is_modified = true;
}

/* 未保存の変更があるか確認する */
bool editor_state_has_unsaved_changes(const EditorState *state) {
  if (state == NULL) {
    return false;
  }

  return state->is_modified;
}

/* エディタの状態を解放する */
void editor_state_free(EditorState *state) {
  if (state == NULL) {
    return;
  }

  free(state->current_path);

  state->current_path = NULL;
  state->is_modified = false;
}

/* 現在開いているファイルのパスを登録する */
bool editor_state_set_path(EditorState *state, const wchar_t *path) {
  if (state == NULL || path == NULL) {
    return false;
  }

  /* ファイルパスの長さを取得 */
  size_t length = wcslen(path);

  /* メモリ確保時のサイズ計算があふれないか確認 */
  if (length >= SIZE_MAX / sizeof(wchar_t)) {
    return false;
  }

  /* 新しいファイルパスを保存するメモリを確保 */
  wchar_t *new_path = malloc((length + 1) * sizeof(wchar_t));

  if (new_path == NULL) {
    return false;
  }

  /* 新しいファイルパスをコピー */
  wmemcpy(new_path, path, length + 1);

  /* 古いファイルパスのメモリを解放 */
  free(state->current_path);

  /* 新しいファイルパスを登録 */
  state->current_path = new_path;

  return true;
}
/* 別のファイルを開く前に確認が必要か判定する */
bool editor_state_requires_confirmation(const EditorState *state) {
  return editor_state_has_unsaved_changes(state);
}
