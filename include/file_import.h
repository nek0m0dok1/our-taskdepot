
#ifndef FILE_IMPORT_H
#define FILE_IMPORT_H

#include <stddef.h>
#include <wchar.h>

/* ファイル読み込みの結果 */
typedef enum {
  FILE_IMPORT_OK,
  FILE_IMPORT_INVALID_ARGUMENT,
  FILE_IMPORT_OPEN_FAILED,
  FILE_IMPORT_READ_FAILED,
  FILE_IMPORT_TOO_LARGE,
  FILE_IMPORT_INVALID_ENCODING,
  FILE_IMPORT_OUT_OF_MEMORY
} FileImportStatus;

/* 読み込んだファイルの情報 */
typedef struct {
  char *content;
  size_t length;
} FileImportResult;

/* UTF-8形式のCソースファイルを読み込む */
FileImportStatus file_import_read(const wchar_t *path, FileImportResult *result);

/* 読み込み結果のメモリを解放する */
void file_import_free(FileImportResult *result);

#endif
