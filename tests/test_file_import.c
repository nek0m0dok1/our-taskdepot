
#include "file_import.h"
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* テストで使用するファイル名 */
#define TEST_FILE L"test_import_sample.c"
#define TEST_JAPANESE_FILE L"test 日本語 file.c"

/* テスト前に実行 */
void setUp(void) {
}

/* テスト後に実行 */
void tearDown(void) {
}

/* テスト用ファイルを作成する */
static void create_test_file(const wchar_t *path, const char *content) {
  FILE *file = NULL;

  TEST_ASSERT_EQUAL_INT(0, _wfopen_s(&file, path, L"wb"));
  TEST_ASSERT_NOT_NULL(file);

  size_t length = strlen(content);
  TEST_ASSERT_EQUAL_UINT(length, fwrite(content, 1, length, file));

  TEST_ASSERT_EQUAL_INT(0, fclose(file));
}

/* UTF-8ファイルを正しく読み込めるか */
void test_read_utf8_file(void) {
  const char *source = "int main(void) { return 0; }\n";
  create_test_file(TEST_FILE, source);

  FileImportResult result = {NULL, 0};
  FileImportStatus status = file_import_read(TEST_FILE, &result);

  TEST_ASSERT_EQUAL(FILE_IMPORT_OK, status);
  TEST_ASSERT_NOT_NULL(result.content);
  TEST_ASSERT_EQUAL_STRING(source, result.content);
  TEST_ASSERT_EQUAL_UINT(strlen(source), result.length);

  file_import_free(&result);
  TEST_ASSERT_NULL(result.content);
  TEST_ASSERT_EQUAL_UINT(0, result.length);

  _wremove(TEST_FILE);
}

/* 日本語と空白を含むパスでも読み込めるか */
void test_japanese_path(void) {
  const char *source = "int value = 10;\n";
  create_test_file(TEST_JAPANESE_FILE, source);

  FileImportResult result = {NULL, 0};
  FileImportStatus status = file_import_read(TEST_JAPANESE_FILE, &result);

  TEST_ASSERT_EQUAL(FILE_IMPORT_OK, status);
  TEST_ASSERT_EQUAL_STRING(source, result.content);

  file_import_free(&result);
  _wremove(TEST_JAPANESE_FILE);
}

/* 存在しないファイルを指定した場合 */
void test_nonexistent_file(void) {
  FileImportResult result = {NULL, 0};

  FileImportStatus status = file_import_read(L"nonexistent_12345.c", &result);

  TEST_ASSERT_EQUAL(FILE_IMPORT_OPEN_FAILED, status);
  TEST_ASSERT_NULL(result.content);
}

/* NULLの引数を渡した場合 */
void test_null_arguments(void) {
  FileImportResult result = {NULL, 0};

  TEST_ASSERT_EQUAL(FILE_IMPORT_INVALID_ARGUMENT, file_import_read(NULL, &result));
  TEST_ASSERT_EQUAL(FILE_IMPORT_INVALID_ARGUMENT, file_import_read(L"sample.c", NULL));
}

/* UTF-8として不正なデータを拒否するか */
void test_invalid_utf8(void) {
  FILE *file = NULL;
  TEST_ASSERT_EQUAL_INT(0, _wfopen_s(&file, TEST_FILE, L"wb"));
  TEST_ASSERT_NOT_NULL(file);

  const unsigned char invalid_bytes[] = {0xC3, 0x28};
  TEST_ASSERT_EQUAL_UINT(sizeof(invalid_bytes),
                         fwrite(invalid_bytes, 1, sizeof(invalid_bytes), file));
  TEST_ASSERT_EQUAL_INT(0, fclose(file));

  FileImportResult result = {NULL, 0};
  FileImportStatus status = file_import_read(TEST_FILE, &result);

  TEST_ASSERT_EQUAL(FILE_IMPORT_INVALID_ENCODING, status);
  TEST_ASSERT_NULL(result.content);

  _wremove(TEST_FILE);
}

/* ファイル読み込みで元のファイルが変更されないか */
void test_original_file_unchanged(void) {
  const char *source = "int original = 1;\n";
  create_test_file(TEST_FILE, source);

  FileImportResult result = {NULL, 0};

  TEST_ASSERT_EQUAL(FILE_IMPORT_OK, file_import_read(TEST_FILE, &result));

  /* メモリ上の内容だけ変更する */
  result.content[0] = 'X';

  FILE *file = NULL;
  TEST_ASSERT_EQUAL_INT(0, _wfopen_s(&file, TEST_FILE, L"rb"));
  TEST_ASSERT_NOT_NULL(file);

  char actual[64] = {0};
  size_t read_size = fread(actual, 1, sizeof(actual) - 1, file);
  TEST_ASSERT_EQUAL_UINT(strlen(source), read_size);
  TEST_ASSERT_EQUAL_STRING(source, actual);

  fclose(file);
  file_import_free(&result);
  _wremove(TEST_FILE);
}

/* BOM付きUTF-8ファイルを正しく読み込めるか */
void test_utf8_with_bom(void) {
  FILE *file = NULL;
  TEST_ASSERT_EQUAL_INT(0, _wfopen_s(&file, TEST_FILE, L"wb"));
  TEST_ASSERT_NOT_NULL(file);

  const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
  const char *source = "int main(void) { return 0; }\n";

  TEST_ASSERT_EQUAL_UINT(sizeof(bom), fwrite(bom, 1, sizeof(bom), file));
  TEST_ASSERT_EQUAL_UINT(strlen(source), fwrite(source, 1, strlen(source), file));
  TEST_ASSERT_EQUAL_INT(0, fclose(file));

  FileImportResult result = {NULL, 0};
  FileImportStatus status = file_import_read(TEST_FILE, &result);

  TEST_ASSERT_EQUAL(FILE_IMPORT_OK, status);
  TEST_ASSERT_NOT_NULL(result.content);
  TEST_ASSERT_EQUAL_STRING(source, result.content);
  TEST_ASSERT_EQUAL_UINT(strlen(source), result.length);

  file_import_free(&result);
  _wremove(TEST_FILE);
}

/* 空のCファイルを正常に読み込めるか */
void test_empty_file(void) {
  FILE *file = NULL;

  /* 中身が空のファイルを作成 */
  TEST_ASSERT_EQUAL_INT(0, _wfopen_s(&file, TEST_FILE, L"wb"));
  TEST_ASSERT_NOT_NULL(file);
  TEST_ASSERT_EQUAL_INT(0, fclose(file));

  FileImportResult result = {NULL, 0};

  FileImportStatus status = file_import_read(TEST_FILE, &result);

  /* 空ファイルも読み込み成功とする */
  TEST_ASSERT_EQUAL(FILE_IMPORT_OK, status);
  TEST_ASSERT_NOT_NULL(result.content);
  TEST_ASSERT_EQUAL_UINT(0, result.length);
  TEST_ASSERT_EQUAL_STRING("", result.content);

  file_import_free(&result);
  _wremove(TEST_FILE);
}

/* 4MiBを超えるファイルを拒否できるか */
void test_file_too_large(void) {
  FILE *file = NULL;

  TEST_ASSERT_EQUAL_INT(0, _wfopen_s(&file, TEST_FILE, L"wb"));
  TEST_ASSERT_NOT_NULL(file);

  /* 4MiBを1バイト超えるファイルを作成 */
  TEST_ASSERT_EQUAL_INT(0, fseek(file, 4L * 1024L * 1024L, SEEK_SET));
  TEST_ASSERT_EQUAL_INT('A', fputc('A', file));
  TEST_ASSERT_EQUAL_INT(0, fclose(file));

  FileImportResult result = {NULL, 0};
  FileImportStatus status = file_import_read(TEST_FILE, &result);

  /* サイズ超過として拒否されることを確認 */
  TEST_ASSERT_EQUAL(FILE_IMPORT_TOO_LARGE, status);
  TEST_ASSERT_NULL(result.content);
  TEST_ASSERT_EQUAL_UINT(0, result.length);

  _wremove(TEST_FILE);
}

/* すべてのテストを実行 */
int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_read_utf8_file);
  RUN_TEST(test_japanese_path);
  RUN_TEST(test_nonexistent_file);
  RUN_TEST(test_null_arguments);
  RUN_TEST(test_invalid_utf8);
  RUN_TEST(test_original_file_unchanged);
  RUN_TEST(test_utf8_with_bom);
  RUN_TEST(test_empty_file);
  RUN_TEST(test_file_too_large);

  return UNITY_END();
}
