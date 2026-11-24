#ifndef COMPILER_DIAGNOSTICS_H
#define COMPILER_DIAGNOSTICS_H

#include "compiler/compiler.h"

#include <Windows.h>

#define COMPILER_LOG_LIMIT (1024u * 1024u)

void compiler_set_system_error(CompileResult *result, DWORD error);
int compiler_set_diagnostics(CompileResult *result, char *bytes, size_t length);
BOOL compiler_read_log(HANDLE pipe, char *log, size_t *length, int *truncated, BOOL *read_data);

#endif
