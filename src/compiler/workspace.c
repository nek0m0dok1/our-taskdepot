#include "workspace.h"
#include "compiler/compiler.h"

#include <objbase.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#include <wchar.h>

#define RECOVERY_LIMIT_MS 3000u
#define WORKSPACE_NAME_LENGTH 46u
#define OWNER_SUFFIX L".owner"

struct CompileWorkspace {
  HANDLE root;
  HANDLE directory;
  HANDLE marker;
  wchar_t dir[MAX_PATH + 1];
  wchar_t source[MAX_PATH + 1];
  wchar_t output[MAX_PATH + 1];
  wchar_t marker_path[MAX_PATH + 1];
  wchar_t scratch[MAX_PATH + 1];
  struct CompileWorkspace *next;
  BOOL work_files_removed;
};

static const char marker_content[] = "our-taskdepot compiler workspace v1";
static SRWLOCK outputs_lock = SRWLOCK_INIT;
static CompileWorkspace *outputs;
static CompileWorkspace *deferred_workspaces;

static HANDLE open_directory(const wchar_t *path, DWORD access) {
  HANDLE directory =
      CreateFileW(path, FILE_READ_ATTRIBUTES | access, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                  OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
  if (directory != INVALID_HANDLE_VALUE) {
    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle(directory, &info) ||
        !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
        (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
      CloseHandle(directory);
      SetLastError(ERROR_DIRECTORY);
      return INVALID_HANDLE_VALUE;
    }
  }
  return directory;
}

static HANDLE open_root(wchar_t *path, size_t capacity) {
  wchar_t temp[MAX_PATH + 1];
  DWORD length = GetTempPath2W(_countof(temp), temp);
  if (length == 0 || length >= _countof(temp) ||
      FAILED(StringCchPrintfW(path, capacity, L"%lsour-taskdepot", temp))) {
    SetLastError(ERROR_BAD_PATHNAME);
    return INVALID_HANDLE_VALUE;
  }
  if (!CreateDirectoryW(path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
    return INVALID_HANDLE_VALUE;
  }
  return open_directory(path, FILE_LIST_DIRECTORY | FILE_ADD_SUBDIRECTORY);
}

static BOOL set_paths(CompileWorkspace *workspace, const wchar_t *root, const wchar_t *name) {
  return SUCCEEDED(
             StringCchPrintfW(workspace->dir, _countof(workspace->dir), L"%ls\\%ls", root, name)) &&
         SUCCEEDED(StringCchPrintfW(workspace->source, _countof(workspace->source),
                                    L"%ls\\source.c", workspace->dir)) &&
         SUCCEEDED(StringCchPrintfW(workspace->output, _countof(workspace->output),
                                    L"%ls\\output.exe", workspace->dir)) &&
         SUCCEEDED(StringCchPrintfW(workspace->marker_path, _countof(workspace->marker_path),
                                    L"%ls" OWNER_SUFFIX, workspace->dir)) &&
         SUCCEEDED(StringCchPrintfW(workspace->scratch, _countof(workspace->scratch),
                                    L"%ls\\scratch", workspace->dir));
}

static BOOL delete_regular_file(const wchar_t *path) {
  HANDLE file = CreateFileW(path, DELETE | FILE_READ_ATTRIBUTES, 0, NULL, OPEN_EXISTING,
                            FILE_FLAG_OPEN_REPARSE_POINT, NULL);
  if (file == INVALID_HANDLE_VALUE) {
    DWORD error = GetLastError();
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
  }
  BY_HANDLE_FILE_INFORMATION info;
  FILE_DISPOSITION_INFO disposition = {TRUE};
  BOOL deleted =
      GetFileInformationByHandle(file, &info) &&
      !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) &&
      SetFileInformationByHandle(file, FileDispositionInfo, &disposition, sizeof(disposition));
  CloseHandle(file);
  return deleted;
}

static BOOL before_deadline(ULONGLONG deadline) {
  return deadline == 0 || GetTickCount64() < deadline;
}

static BOOL clean_scratch(const wchar_t *scratch, ULONGLONG deadline) {
  if (!before_deadline(deadline)) {
    return FALSE;
  }
  HANDLE directory = open_directory(scratch, FILE_LIST_DIRECTORY);
  if (directory == INVALID_HANDLE_VALUE) {
    DWORD error = GetLastError();
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
  }

  wchar_t path[MAX_PATH + 1];
  BOOL clean = SUCCEEDED(StringCchPrintfW(path, _countof(path), L"%ls\\*", scratch));
  if (clean) {
    WIN32_FIND_DATAW entry;
    HANDLE search = FindFirstFileW(path, &entry);
    if (search == INVALID_HANDLE_VALUE) {
      clean = GetLastError() == ERROR_FILE_NOT_FOUND;
    } else {
      do {
        if (!before_deadline(deadline)) {
          clean = FALSE;
          break;
        }
        if (wcscmp(entry.cFileName, L".") != 0 && wcscmp(entry.cFileName, L"..") != 0) {
          if ((entry.dwFileAttributes &
               (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ||
              FAILED(
                  StringCchPrintfW(path, _countof(path), L"%ls\\%ls", scratch, entry.cFileName)) ||
              !delete_regular_file(path)) {
            clean = FALSE;
          }
        }
        if (!FindNextFileW(search, &entry)) {
          clean = clean && GetLastError() == ERROR_NO_MORE_FILES;
          break;
        }
      } while (TRUE);
      FindClose(search);
    }
  }
  CloseHandle(directory);
  return clean && before_deadline(deadline) && RemoveDirectoryW(scratch);
}

static BOOL close_marker(HANDLE marker, BOOL remove) {
  FILE_DISPOSITION_INFO disposition = {TRUE};
  BOOL removed = remove && SetFileInformationByHandle(marker, FileDispositionInfo, &disposition,
                                                      sizeof(disposition));
  CloseHandle(marker);
  return removed;
}

static BOOL remove_workspace(CompileWorkspace *workspace, BOOL files_deleted, ULONGLONG deadline) {
  CloseHandle(workspace->directory);
  BOOL removed = files_deleted && before_deadline(deadline) && RemoveDirectoryW(workspace->dir);
  return close_marker(workspace->marker, removed && before_deadline(deadline));
}

CompileWorkspace *compile_workspace_create(void) {
  wchar_t root[MAX_PATH + 1];
  HANDLE root_handle = open_root(root, _countof(root));
  if (root_handle == INVALID_HANDLE_VALUE) {
    return NULL;
  }

  CompileWorkspace *workspace = calloc(1, sizeof(*workspace));
  GUID guid;
  wchar_t name[WORKSPACE_NAME_LENGTH + 1] = L"compile-";
  if (workspace == NULL || FAILED(CoCreateGuid(&guid)) || !StringFromGUID2(&guid, name + 8, 39) ||
      !set_paths(workspace, root, name)) {
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    goto failed;
  }
  workspace->marker = CreateFileW(workspace->marker_path, GENERIC_WRITE | DELETE, 0, NULL,
                                  CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
  if (workspace->marker == INVALID_HANDLE_VALUE) {
    goto failed;
  }
  DWORD written;
  BOOL created =
      WriteFile(workspace->marker, marker_content, sizeof(marker_content), &written, NULL) &&
      written == sizeof(marker_content) && CreateDirectoryW(workspace->dir, NULL);
  if (created) {
    workspace->directory = open_directory(workspace->dir, 0);
    if (workspace->directory != INVALID_HANDLE_VALUE &&
        CreateDirectoryW(workspace->scratch, NULL)) {
      workspace->root = root_handle;
      return workspace;
    }
  }

  DWORD create_error = GetLastError();
  if (created && workspace->directory != INVALID_HANDLE_VALUE) {
    remove_workspace(workspace, TRUE, 0);
  } else {
    close_marker(workspace->marker, !created);
  }
  SetLastError(create_error);

failed: {
  DWORD error = GetLastError();
  CloseHandle(root_handle);
  free(workspace);
  SetLastError(error);
}
  return NULL;
}

const wchar_t *compile_workspace_dir(const CompileWorkspace *workspace) {
  return workspace->dir;
}

const wchar_t *compile_workspace_source(const CompileWorkspace *workspace) {
  return workspace->source;
}

const wchar_t *compile_workspace_output(const CompileWorkspace *workspace) {
  return workspace->output;
}

const wchar_t *compile_workspace_scratch(const CompileWorkspace *workspace) {
  return workspace->scratch;
}

BOOL compile_workspace_finish(CompileWorkspace *workspace, BOOL keep_output) {
  BOOL source_deleted = delete_regular_file(workspace->source);
  BOOL scratch_deleted = clean_scratch(workspace->scratch, 0);
  workspace->work_files_removed = source_deleted && scratch_deleted;
  if (keep_output) {
    AcquireSRWLockExclusive(&outputs_lock);
    workspace->next = outputs;
    outputs = workspace;
    ReleaseSRWLockExclusive(&outputs_lock);
    return workspace->work_files_removed;
  }
  BOOL output_deleted = delete_regular_file(workspace->output);
  BOOL removed = remove_workspace(workspace, workspace->work_files_removed && output_deleted, 0);
  CloseHandle(workspace->root);
  free(workspace);
  return removed;
}

void compile_workspace_defer(CompileWorkspace *workspace) {
  // 停止未確認の作業領域はプロセス終了まで回収対象にしない。
  AcquireSRWLockExclusive(&outputs_lock);
  workspace->next = deferred_workspaces;
  deferred_workspaces = workspace;
  ReleaseSRWLockExclusive(&outputs_lock);
}

int compile_output_delete(const wchar_t *output_path) {
  if (output_path == NULL) {
    return 0;
  }

  AcquireSRWLockExclusive(&outputs_lock);
  CompileWorkspace **entry = &outputs;
  while (*entry != NULL && wcscmp((*entry)->output, output_path) != 0) {
    entry = &(*entry)->next;
  }
  CompileWorkspace *workspace = *entry;
  if (workspace != NULL) {
    *entry = workspace->next;
  }
  ReleaseSRWLockExclusive(&outputs_lock);
  if (workspace == NULL) {
    return 0;
  }
  BOOL output_deleted = delete_regular_file(workspace->output);
  BOOL removed = remove_workspace(workspace, workspace->work_files_removed && output_deleted, 0);
  CloseHandle(workspace->root);
  free(workspace);
  return removed;
}

static BOOL is_workspace_name(const wchar_t *name) {
  if (wcslen(name) != WORKSPACE_NAME_LENGTH || wcsncmp(name, L"compile-", 8) != 0) {
    return FALSE;
  }
  GUID guid;
  wchar_t canonical[39];
  return SUCCEEDED(CLSIDFromString(name + 8, &guid)) &&
         StringFromGUID2(&guid, canonical, _countof(canonical)) &&
         _wcsicmp(canonical, name + 8) == 0;
}

static BOOL recover_workspace(const wchar_t *root, const wchar_t *name, ULONGLONG deadline) {
  CompileWorkspace workspace = {0};
  if (!set_paths(&workspace, root, name)) {
    return FALSE;
  }
  workspace.marker = CreateFileW(workspace.marker_path, GENERIC_READ | DELETE, 0, NULL,
                                 OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
  if (workspace.marker == INVALID_HANDLE_VALUE) {
    DWORD error = GetLastError();
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_SHARING_VIOLATION;
  }

  BY_HANDLE_FILE_INFORMATION info;
  char stamp[sizeof(marker_content) + 1];
  DWORD read;
  if (!GetFileInformationByHandle(workspace.marker, &info) ||
      (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ||
      !ReadFile(workspace.marker, stamp, sizeof(stamp), &read, NULL) ||
      read != sizeof(marker_content) ||
      memcmp(stamp, marker_content, sizeof(marker_content)) != 0) {
    CloseHandle(workspace.marker);
    return TRUE;
  }

  workspace.directory = open_directory(workspace.dir, 0);
  if (workspace.directory == INVALID_HANDLE_VALUE) {
    DWORD error = GetLastError();
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
      return close_marker(workspace.marker, before_deadline(deadline));
    }
    CloseHandle(workspace.marker);
    return FALSE;
  }

  BOOL source_deleted = before_deadline(deadline) && delete_regular_file(workspace.source);
  BOOL output_deleted = before_deadline(deadline) && delete_regular_file(workspace.output);
  BOOL scratch_deleted = clean_scratch(workspace.scratch, deadline);
  return remove_workspace(&workspace, source_deleted && output_deleted && scratch_deleted,
                          deadline);
}

int compile_recover_workspaces(void) {
  ULONGLONG deadline = GetTickCount64() + RECOVERY_LIMIT_MS;
  wchar_t root[MAX_PATH + 1];
  HANDLE root_handle = open_root(root, _countof(root));
  if (root_handle == INVALID_HANDLE_VALUE) {
    return -1;
  }
  wchar_t pattern[MAX_PATH + 1];
  if (FAILED(StringCchPrintfW(pattern, _countof(pattern), L"%ls\\compile-*" OWNER_SUFFIX, root))) {
    CloseHandle(root_handle);
    return -1;
  }

  int result = 0;
  WIN32_FIND_DATAW entry;
  HANDLE search = FindFirstFileW(pattern, &entry);
  if (search == INVALID_HANDLE_VALUE) {
    result = GetLastError() != ERROR_FILE_NOT_FOUND;
  } else {
    do {
      if (!before_deadline(deadline)) {
        result = 1;
        break;
      }
      if (!(entry.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) &&
          wcslen(entry.cFileName) == WORKSPACE_NAME_LENGTH + _countof(OWNER_SUFFIX) - 1 &&
          wcscmp(entry.cFileName + WORKSPACE_NAME_LENGTH, OWNER_SUFFIX) == 0) {
        entry.cFileName[WORKSPACE_NAME_LENGTH] = L'\0';
        if (is_workspace_name(entry.cFileName) &&
            !recover_workspace(root, entry.cFileName, deadline)) {
          result = 1;
        }
      }
      if (!FindNextFileW(search, &entry)) {
        if (GetLastError() != ERROR_NO_MORE_FILES) {
          result = 1;
        }
        break;
      }
    } while (TRUE);
    FindClose(search);
  }
  CloseHandle(root_handle);
  return result;
}
