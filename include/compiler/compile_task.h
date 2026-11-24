#ifndef COMPILE_TASK_H
#define COMPILE_TASK_H

#include "compiler/compiler.h"

typedef struct CompileTask CompileTask;

// ソースと設定をコピーしてワーカーを開始する。開始できなければ NULL。
CompileTask *compile_task_start(const char *source, const CompileOptions *options);

// 完了時にシグナル状態になる借用 HANDLE。呼び出し側は閉じない。
void *compile_task_completion_handle(const CompileTask *task);

// 中断を要求する。完了済みなら確定した結果は変わらない。
void compile_task_cancel(CompileTask *task);

// 完了済みの結果を一度だけ out へ移す。未完了なら待たずに 0 を返す。
int compile_task_take_result(CompileTask *task, CompileResult *out);

// 中断を要求してワーカーを回収する。未取得の結果と出力も破棄する。
// タスクの操作と破棄は同じ呼び出し側スレッドで行う。
void compile_task_destroy(CompileTask *task);

#endif
