#ifndef COMPILER_WORKSPACE_H
#define COMPILER_WORKSPACE_H

#include <Windows.h>

typedef struct CompileWorkspace CompileWorkspace;

CompileWorkspace *compile_workspace_create(void);
const wchar_t *compile_workspace_dir(const CompileWorkspace *workspace);
const wchar_t *compile_workspace_source(const CompileWorkspace *workspace);
const wchar_t *compile_workspace_output(const CompileWorkspace *workspace);
const wchar_t *compile_workspace_scratch(const CompileWorkspace *workspace);
BOOL compile_workspace_finish(CompileWorkspace *workspace, BOOL keep_output);
void compile_workspace_defer(CompileWorkspace *workspace);

#endif
