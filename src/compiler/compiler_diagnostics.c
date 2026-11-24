#include "compiler_diagnostics.h"

#include <stdlib.h>
#include <string.h>
#include <strsafe.h>

void compiler_set_system_error(CompileResult *result, DWORD error) {
  wchar_t message[512];
  DWORD length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                                error, 0, message, _countof(message), NULL);
  if (length == 0) {
    StringCchPrintfW(message, _countof(message), L"Windows error %lu", error);
  }

  char utf8[sizeof(message) * 2];
  if (WideCharToMultiByte(CP_UTF8, 0, message, -1, utf8, sizeof(utf8), NULL, NULL)) {
    char *copy = _strdup(utf8);
    if (copy != NULL) {
      free(result->diagnostics);
      result->diagnostics = copy;
    }
  }
}

// Windows の UTF-8 デコーダーで不正なシーケンスを U+FFFD に置換する。
int compiler_set_diagnostics(CompileResult *result, char *bytes, size_t length) {
  if (length == 0) {
    return 1;
  }
  if (result->logs_truncated) {
    size_t start = length - 1;
    while (start > 0 && length - start < 4 && (bytes[start] & 0xc0) == 0x80) {
      --start;
    }
    unsigned char lead = (unsigned char)bytes[start];
    size_t width = lead >= 0xf0 && lead <= 0xf4   ? 4
                   : lead >= 0xe0 && lead <= 0xef ? 3
                   : lead >= 0xc2 && lead <= 0xdf ? 2
                                                  : 1;
    if (width > length - start) {
      length = start;
    }
    if (length == 0) {
      return 1;
    }
  }

  for (size_t i = 0; i < length; ++i) {
    if (bytes[i] == '\0') {
      bytes[i] = (char)0xff;
    }
  }
  int wide_length = MultiByteToWideChar(CP_UTF8, 0, bytes, (int)length, NULL, 0);
  if (wide_length == 0) {
    return 0;
  }
  wchar_t *wide = malloc((size_t)wide_length * sizeof(*wide));
  if (wide == NULL) {
    return 0;
  }

  int ok = 0;
  if (MultiByteToWideChar(CP_UTF8, 0, bytes, (int)length, wide, wide_length)) {
    int size = WideCharToMultiByte(CP_UTF8, 0, wide, wide_length, NULL, 0, NULL, NULL);
    char *utf8 = size > 0 ? malloc((size_t)size + 1) : NULL;
    if (utf8 != NULL &&
        WideCharToMultiByte(CP_UTF8, 0, wide, wide_length, utf8, size, NULL, NULL)) {
      if (size > COMPILER_LOG_LIMIT) {
        size = COMPILER_LOG_LIMIT;
        while ((utf8[size] & 0xc0) == 0x80) {
          --size;
        }
        result->logs_truncated = 1;
      }
      utf8[size] = '\0';
      free(result->diagnostics);
      result->diagnostics = utf8;
      ok = 1;
    } else {
      free(utf8);
    }
  }
  free(wide);
  return ok;
}

BOOL compiler_read_log(HANDLE pipe, char *log, size_t *length, int *truncated, BOOL *read_data) {
  DWORD available;
  *read_data = FALSE;
  if (!PeekNamedPipe(pipe, NULL, 0, NULL, &available, NULL)) {
    return GetLastError() == ERROR_BROKEN_PIPE;
  }
  if (available == 0) {
    return TRUE;
  }

  char buffer[8192];
  DWORD wanted = available < sizeof(buffer) ? available : sizeof(buffer);
  DWORD read;
  if (!ReadFile(pipe, buffer, wanted, &read, NULL)) {
    return GetLastError() == ERROR_BROKEN_PIPE;
  }
  size_t remaining = COMPILER_LOG_LIMIT - *length;
  size_t saved = read < remaining ? read : remaining;
  memcpy(log + *length, buffer, saved);
  *length += saved;
  *truncated |= saved < read;
  *read_data = read != 0;
  return TRUE;
}
