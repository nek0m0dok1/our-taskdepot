#include "file_dialog.h"

#include <commdlg.h>
#include <string.h>

/* Windows標準のファイル選択ダイアログを開く */
FileDialogStatus file_dialog_open(HWND owner, wchar_t *path, size_t path_capacity) {

  /* 選択したファイルパスを一時的に保存する */
  wchar_t selected_path[32768] = {0};

  /* Windowsのファイル選択ダイアログの設定 */
  OPENFILENAMEW dialog = {0};

  /* 引数が不正な場合 */
  if (path == NULL || path_capacity == 0) {
    return FILE_DIALOG_INVALID_ARGUMENT;
  }

  /* 構造体のサイズ */
  dialog.lStructSize = sizeof(dialog);

  /* 親ウィンドウ */
  dialog.hwndOwner = owner;

  /* 選択できるファイルの種類 */
  dialog.lpstrFilter = L"C Source Files (*.c)\0*.c\0All Files (*.*)\0*.*\0";

  /* 選択したファイルパスの保存先 */
  dialog.lpstrFile = selected_path;

  /* 保存先の文字数 */
  dialog.nMaxFile = (DWORD)(sizeof(selected_path) / sizeof(selected_path[0]));

  /* ダイアログのタイトル */
  dialog.lpstrTitle = L"Cソースファイルを開く";

  /* ファイルが存在することを確認する */
  dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

  /* ファイル選択ダイアログを表示する */
  if (GetOpenFileNameW(&dialog) == FALSE) {

    /* 利用者がキャンセルした場合 */
    if (CommDlgExtendedError() == 0) {
      return FILE_DIALOG_CANCELLED;
    }

    /* その他のエラー */
    return FILE_DIALOG_ERROR;
  }

  /* 呼び出し元の領域にパスが収まるか確認する */
  size_t selected_length = wcslen(selected_path);

  if (selected_length >= path_capacity) {
    return FILE_DIALOG_INVALID_ARGUMENT;
  }

  /* 選択したファイルパスをコピーする */
  if (wcscpy_s(path, path_capacity, selected_path) != 0) {
    return FILE_DIALOG_ERROR;
  }

  return FILE_DIALOG_OK;
}
