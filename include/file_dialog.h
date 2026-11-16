#ifndef FILE_DIALOG_H
#define FILE_DIALOG_H

#include <stddef.h>
#include <wchar.h>
#include <windows.h>

/* ファイル選択の結果 */
typedef enum {
  FILE_DIALOG_OK,
  FILE_DIALOG_CANCELLED,
  FILE_DIALOG_ERROR,
  FILE_DIALOG_INVALID_ARGUMENT
} FileDialogStatus;

/* Windows標準のファイル選択ダイアログを開く */
FileDialogStatus file_dialog_open(HWND owner, wchar_t *path, size_t path_capacity);

#endif
