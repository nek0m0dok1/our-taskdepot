#ifndef EDITOR_STATE_H
#define EDITOR_STATE_H

#include <stdbool.h>
#include <wchar.h>

/* エディタの状態 */
typedef struct {
  wchar_t *current_path;
  bool is_modified;
} EditorState;

/* エディタの状態を初期化する */
void editor_state_init(EditorState *state);

/* 編集されたことを記録する */
void editor_state_mark_modified(EditorState *state);

/* 未保存の変更があるか確認する */
bool editor_state_has_unsaved_changes(const EditorState *state);

/* エディタの状態を解放する */
void editor_state_free(EditorState *state);

/* 現在開いているファイルのパスを登録する */
bool editor_state_set_path(EditorState *state, const wchar_t *path);

/* 別のファイルを開く前に確認が必要か判定する */
bool editor_state_requires_confirmation(const EditorState *state);

#endif
