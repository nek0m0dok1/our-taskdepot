#include "compiler/compile_task.h"

#include <Windows.h>
#include <process.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

struct CompileTask {
  HANDLE worker;
  HANDLE cancel_event;
  char *source;
  wchar_t *compiler_path;
  CompileOptions options;
  CompileResult result;
  int result_taken;
};

static unsigned __stdcall compile_worker(void *context) {
  CompileTask *task = context;
  task->result = compile_source_with_cancel(task->source, &task->options, task->cancel_event);
  return 0;
}

CompileTask *compile_task_start(const char *source, const CompileOptions *options) {
  if (source == NULL) {
    return NULL;
  }
  CompileTask *task = calloc(1, sizeof(*task));
  if (task == NULL) {
    return NULL;
  }
  task->source = _strdup(source);
  if (task->source == NULL) {
    compile_task_destroy(task);
    return NULL;
  }
  if (options != NULL) {
    task->options.timeout_ms = options->timeout_ms;
    if (options->compiler_path != NULL) {
      task->compiler_path = _wcsdup(options->compiler_path);
      if (task->compiler_path == NULL) {
        compile_task_destroy(task);
        return NULL;
      }
      task->options.compiler_path = task->compiler_path;
    }
  }
  task->cancel_event = CreateEventW(NULL, TRUE, FALSE, NULL);
  if (task->cancel_event == NULL) {
    compile_task_destroy(task);
    return NULL;
  }
  task->worker = (HANDLE)_beginthreadex(NULL, 0, compile_worker, task, 0, NULL);
  if (task->worker == NULL) {
    compile_task_destroy(task);
    return NULL;
  }
  return task;
}

void *compile_task_completion_handle(const CompileTask *task) {
  return task != NULL ? task->worker : NULL;
}

void compile_task_cancel(CompileTask *task) {
  if (task != NULL && task->cancel_event != NULL) {
    SetEvent(task->cancel_event);
  }
}

int compile_task_take_result(CompileTask *task, CompileResult *out) {
  if (task == NULL || out == NULL || task->result_taken ||
      WaitForSingleObject(task->worker, 0) != WAIT_OBJECT_0) {
    return 0;
  }
  *out = task->result;
  task->result = (CompileResult){0};
  task->result_taken = 1;
  return 1;
}

void compile_task_destroy(CompileTask *task) {
  if (task == NULL) {
    return;
  }
  if (task->worker != NULL) {
    compile_task_cancel(task);
    WaitForSingleObject(task->worker, INFINITE);
    CloseHandle(task->worker);
  }
  if (task->cancel_event != NULL) {
    CloseHandle(task->cancel_event);
  }
  if (task->result.output_path != NULL) {
    compile_output_delete(task->result.output_path);
  }
  compile_result_free(&task->result);
  free(task->compiler_path);
  free(task->source);
  free(task);
}
