/*
 ファイル名: compiler.c
 用途: 入力ソースコードのコンパイル処理に使用
 内容: 型と関数の宣言
 */

// ヘッダファイル
#include "compiler/compiler.h"

#include <Windows.h>
#include <corecrt_search.h>
#include <minwindef.h>
#include <objbase.h>
#include <processthreadsapi.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#include <wchar.h>
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

  const wchar_t *compiler_path = NULL;
  wchar_t bundled_path[MAX_PATH + 1];

  // インスタンスメンバ初期化
  CompileResult result = {0};

  // result初期状態
  result.status = COMPILE_INTERNAL_ERROR;

  // 不正な引数の判定・記録
  if (source == NULL) {
    result.status = COMPILE_INVALID_ARGUMENT;
    return result;
  }

  // コンパイラのパスを取得
  const wchar_t *compiler_path = NULL;

  if (options != NULL) { // ユーザーからのパス指定あり
    compiler_path = options->compiler_path;
  }

  if (compiler_path == NULL) { // ユーザーからのパス指定なし(同梱コンパイラ使用)
    wchar_t exe_path[MAX_PATH + 1];

    DWORD exe_length = GetModuleFileNameW(NULL, exe_path, _countof(exe_path));

    // パス長がバッファを超えるとき内部エラーを返す
    if (exe_length == 0 || exe_length >= _countof(exe_path)) {
      return result;
    }

    // 格納された実行ファイルパスからファイル部分を除去(≠消去)
    wchar_t *last_separator = wcsrchr(exe_path, L'\\');

    if (last_separator == NULL) {
      return result;
    }

    // 終端文字書込み
    *last_separator = L'\0';

    // 同梱するClangのパス組み立て
    HRESULT compiler_hr = StringCchPrintfW(bundled_path, _countof(bundled_path),
                                           L"%ls\\compiler\\bin\\clang.exe", exe_path);

    if (FAILED(compiler_hr)) {
      return result;
    }

    // compiler_pathにClangのパスを渡す
    compiler_path = bundled_path;
  }

  // 一時フォルダを取得する処理
  wchar_t temp_dir[MAX_PATH + 1]; // フォルダパス書込み用配列

  DWORD length = GetTempPath2W(_countof(temp_dir), temp_dir);

  if (length == 0 || length >= _countof(temp_dir)) {
    return result;
  }
  // アプリ専用フォルダーのパスを生成
  wchar_t app_dir[MAX_PATH + 1];

  HRESULT hr = StringCchPrintfW(app_dir, _countof(app_dir), L"%lsour-taskdepot", temp_dir);

  if (FAILED(hr)) {
    return result;
  }

  // アプリ専用フォルダーを準備
  if (!ensure_app_temp_dir(app_dir)) {
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

  // コンパイル用フォルダのパス生成
  wchar_t work_dir[MAX_PATH + 1];

  hr = StringCchPrintfW(work_dir, _countof(work_dir), L"%ls\\compile-%ls", app_dir, guid_text);

  if (FAILED(hr)) {
    return result;
  }

  // コンパイル用フォルダを作成
  if (!CreateDirectoryW(work_dir, NULL)) {
    return result;
  }

  // ソースファイルのパスを生成
  wchar_t source_path[MAX_PATH + 1];

  hr = StringCchPrintfW(source_path, _countof(source_path), L"%ls\\source.c", work_dir);

  if (FAILED(hr)) {
    return result;
  }

  // ソースファイルを新規作成
  HANDLE file = CreateFileW(source_path,           // ファイル保存先
                            GENERIC_WRITE,         // 書き込みを許可
                            0,                     // プロセスとの共有を許可しない
                            NULL,                  // 標準セキュリティ
                            CREATE_NEW,            // 新規作成のみ(既存ファイルの上書きを防止)
                            FILE_ATTRIBUTE_NORMAL, // 通常ファイル
                            NULL                   // テンプレートを使わない
  );

  if (file == INVALID_HANDLE_VALUE) {
    return result;
  }

  // sourceのバイト数を取得
  size_t source_length = strlen(source);

  // 書き込みのサイズ確認
  if (source_length > MAXDWORD) {
    CloseHandle(file);
    return result;
  }

  // ファイルへの書き込み
  DWORD written = 0;

  BOOL write_ok = WriteFile(file, source, (DWORD)source_length, &written, NULL);

  // 書き込み結果を確認
  if (!write_ok || written != (DWORD)source_length) {
    CloseHandle(file);
    return result;
  }

  // ファイルを閉じる
  if (!CloseHandle(file)) {
    return result;
  }

  // 出力パス生成
  wchar_t output_path[MAX_PATH + 1]; // 実行ファイル生成場所

  HRESULT output_hr =
      StringCchPrintfW(output_path, _countof(output_path), L"%ls\\output.exe", work_dir);

  if (FAILED(output_hr)) {
    return result;
  }

  // Clangに渡すコマンドライン
  wchar_t command_line[4 * (MAX_PATH + 1)];

  // コマンドを文字配列に格納
  HRESULT command_hr = StringCchPrintfW(command_line, _countof(command_line),
                                        L"\"%ls\" -std=c23 -Wall -Wextra \"%ls\" -o \"%ls\"",
                                        compiler_path, source_path, output_path);

  if (FAILED(command_hr)) {
    return result;
  }

  return result;
}
