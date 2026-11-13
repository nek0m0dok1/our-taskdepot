/*
 ファイル名: compiler.h
 用途: 入力ソースコードのコンパイル処理に使用
 内容: 型と関数の宣言
 */

// インクルードガード
#ifndef COMPILER_H
#define COMPILER_H

// 標準ヘッダー
#include <stddef.h>

// 処理結果の分類
typedef enum {
  COMPILE_SUCCESS,          // 成功
  COMPILE_ERROR,            // コンパイル失敗
  COMPILE_TIMEOUT,          // タイムアウト
  COMPILE_LAUNCH_ERROR,     // コンパイラ起動失敗
  COMPILE_INTERNAL_ERROR,   // Compilerモジュール内部の失敗
  COMPILE_INVALID_ARGUMENT, // 不正な引数
  COMPILE_CANCELLED         // コンパイル強制中断
} CompileStatus;

// コンパイル結果
typedef struct {
  CompileStatus status;    // コンパイル結果の分類
  unsigned long exit_code; // 終了コード
  int has_exit_code;       // 終了コードの取得成否
  char *diagnostics;       // コンパイラからの診断ログ
  int logs_truncated;      // 1MiB以上のログが省略されたか
  wchar_t *output_path;    // コンパイル成功時の一時ファイルのパス
  int cleanup_failed;      // 一時ファイル等の片付け失敗
} CompileResult;

// コンパイル結果が占有する動的メモリ解放
void compile_result_free(CompileResult *result);

// 生成した一時実行ファイルを削除
int compile_output_delete(const wchar_t *output_path);

// コンパイルの設定
typedef struct {
  const wchar_t *compiler_path; // コンパイラのパス
  unsigned int timeout_ms;      // コンパイル処理の許容時間(ms)
} CompileOptions;

// コンパイル結果を返す関数
CompileResult compile_source(const char *source, const CompileOptions *options);

#endif
