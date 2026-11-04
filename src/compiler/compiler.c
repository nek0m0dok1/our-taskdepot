/*
 ファイル名: compiler.c
 用途: 入力ソースコードのコンパイル処理に使用
 内容: 型と関数の宣言
 */

// ヘッダファイル
#include "compiler/compiler.h"
#include <stdlib.h>

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
