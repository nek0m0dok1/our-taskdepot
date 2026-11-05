/*
 ファイル名: compiler.c
 用途: 入力ソースコードのコンパイル処理に使用
 内容: 型と関数の宣言
 */

// ヘッダファイル
#include "compiler/compiler.h"

#include <Windows.h>
#include <combaseapi.h>
#include <corecrt_search.h>
#include <fileapi.h>
#include <minwindef.h>
#include <objbase.h>
#include <stdlib.h>
#include <winnt.h>

// 補助関数：一時ファイル作成
static int ensure_app_temp_dir(const wchar_t *path) {
  if (CreateDirectoryW(path, NULL)) {
    return 1;
  }
  if (GetLastError() != ERROR_ALREADY_EXISTS) {
    return 0;
  }

  DWORD attributes = GetFileAttributesW(path);

  if (attributes == INVALID_FILE_ATTRIBUTES) {
    return 0;
  }
  if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
    return 0;
  }

  if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
    return 0;
  }

  return 1;
}

// resultが占有する動的メモリの解放処理
void compile_result_free(CompileResult *result) {
  // resultが空のとき、何もせず返す
  if (result == NULL) {
    return;
  }

  // メモリ解放
  free(result->diagnostics);
  free(result->output_path);

  // 解放後NULL値を代入
  result->diagnostics = NULL;
  result->output_path = NULL;
}

// コンパイル処理
CompileResult compile_source(const char *source, const CompileOptions *options) {

  // インスタンスメンバ初期化
  CompileResult result = {0};

  // result初期状態
  result.status = COMPILE_INTERNAL_ERROR;

  // 不正な引数の判定・記録
  if (source == NULL) {
    result.status = COMPILE_INVALID_ARGUMENT;
    return result;
  }

  return result;

  // 一時フォルダを取得する処理
  wchar_t temp_dir[MAX_PATH + 1]; // フォルダパス書込み用配列
  // 配列にパスを書き込み
  DWORD length = GetTempPath2W(MAX_PATH + 1, temp_dir);
  // 取得時の内部エラー
  if (length == 0 || length >= MAX_PATH + 1) {
    result.status = COMPILE_INTERNAL_ERROR;
    return result;
  }

  // GUID生成
  GUID guid; // GUID格納用変数

  if (FAILED(CoCreateGuid(&guid))) {
    return result;
  }

  wchar_t guid_text[39];

  if (StringFromGUID2(&guid, guid_text, _countof(guid_text)) == 0) {
    return result;
  }
}
