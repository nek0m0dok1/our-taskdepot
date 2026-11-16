
#include "file_import.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* 読み込み可能な最大ファイルサイズ：4MiB */
#define FILE_IMPORT_MAX_SIZE (4u * 1024u * 1024u)

/* 読み込んだメモリを解放する */
void file_import_free(FileImportResult *result) {
  if (result == NULL) {
    return;
  }

  free(result->content);
  result->content = NULL;
  result->length = 0;
}

/* UTF-8形式のCソースファイルを読み込む */
FileImportStatus file_import_read(const wchar_t *path, FileImportResult *result) {
  FILE *file = NULL;
  char *buffer = NULL;
  long file_size;
  size_t bytes_read;
  size_t offset = 0;
  size_t content_length;

  /* 引数の検証 */
  if (result == NULL) {
    return FILE_IMPORT_INVALID_ARGUMENT;
  }

  /* 失敗時は既存の結果を変更しない */
  if (result->content != NULL) {
    return FILE_IMPORT_INVALID_ARGUMENT;
  }

  result->length = 0;

  if (path == NULL || path[0] == L'\0') {
    return FILE_IMPORT_INVALID_ARGUMENT;
  }

  /* ファイルを読み取り専用で開く */
  if (_wfopen_s(&file, path, L"rb") != 0 || file == NULL) {
    return FILE_IMPORT_OPEN_FAILED;
  }

  /* ファイルサイズを取得 */
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return FILE_IMPORT_READ_FAILED;
  }

  file_size = ftell(file);

  if (file_size < 0) {
    fclose(file);
    return FILE_IMPORT_READ_FAILED;
  }

  if ((unsigned long)file_size > FILE_IMPORT_MAX_SIZE) {
    fclose(file);
    return FILE_IMPORT_TOO_LARGE;
  }

  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return FILE_IMPORT_READ_FAILED;
  }

  /* ファイル内容を保存するメモリを確保 */
  buffer = malloc((size_t)file_size + 1);

  if (buffer == NULL) {
    fclose(file);
    return FILE_IMPORT_OUT_OF_MEMORY;
  }

  /* ファイル内容を読み込む */
  bytes_read = fread(buffer, 1, (size_t)file_size, file);

  if (bytes_read != (size_t)file_size) {
    free(buffer);
    fclose(file);
    return FILE_IMPORT_READ_FAILED;
  }

  /* 読み込み後、ファイルを閉じる */
  fclose(file);

  /* UTF-8のBOMが存在する場合は取り除く */
  if (bytes_read >= 3 && (unsigned char)buffer[0] == 0xEF && (unsigned char)buffer[1] == 0xBB &&
      (unsigned char)buffer[2] == 0xBF) {
    offset = 3;
  }

  content_length = bytes_read - offset;

  if (offset != 0) {
    if (memmove_s(buffer, (size_t)file_size + 1, buffer + offset, content_length) != 0) {
      free(buffer);
      return FILE_IMPORT_READ_FAILED;
    }
  }

  /* C言語の文字列として扱えないNULバイトを拒否 */
  if (memchr(buffer, '\0', content_length) != NULL) {
    free(buffer);
    return FILE_IMPORT_INVALID_ENCODING;
  }

  buffer[content_length] = '\0';

  /* UTF-8の文字コードを検証 */
  if (content_length > 0) {
    int valid =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, buffer, (int)content_length, NULL, 0);

    if (valid == 0) {
      free(buffer);
      return FILE_IMPORT_INVALID_ENCODING;
    }
  }

  /* 正常に読み込めた内容を返す */
  result->content = buffer;
  result->length = content_length;

  return FILE_IMPORT_OK;
}
