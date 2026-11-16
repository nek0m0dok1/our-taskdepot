#include "file_dialog.h"
#include "file_import.h"

#include <stdio.h>

/* ファイル選択と読み込みの手動テスト */
int main(void) {
  wchar_t path[32768] = {0};

  /* ファイル選択ダイアログを開く */
  FileDialogStatus dialog_status = file_dialog_open(NULL, path, sizeof(path) / sizeof(path[0]));

  /* キャンセルされた場合 */
  if (dialog_status == FILE_DIALOG_CANCELLED) {
    printf("File selection cancelled.\n");
    return 0;
  }

  /* ファイル選択に失敗した場合 */
  if (dialog_status != FILE_DIALOG_OK) {
    printf("File selection failed.\n");
    return 1;
  }

  /* ファイルの内容を読み込む */
  FileImportResult result = {NULL, 0};

  FileImportStatus import_status = file_import_read(path, &result);

  /* 読み込みに失敗した場合 */
  if (import_status != FILE_IMPORT_OK) {
    printf("File import failed. Status: %d\n", (int)import_status);
    return 1;
  }

  /* 読み込んだソースコードを表示する */
  printf("----- File content -----\n");

  if (result.length > 0) {
    fwrite(result.content, 1, result.length, stdout);
  }

  printf("\n----- End of file -----\n");

  /* 使用したメモリを解放する */
  file_import_free(&result);

  return 0;
}
