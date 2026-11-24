#ifndef COMPILER_PROCESS_H
#define COMPILER_PROCESS_H

#include "compiler/compiler.h"

#include <Windows.h>

void compiler_run_process(CompileResult *result, const wchar_t *compiler_path,
                          wchar_t *command_line, const wchar_t *work_dir, const wchar_t *scratch,
                          unsigned int timeout_ms, HANDLE cancel_event);

#endif
