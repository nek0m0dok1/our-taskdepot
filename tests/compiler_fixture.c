#include <Windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define LOG_LIMIT (1024u * 1024u)

static int emit(DWORD stream, const char *bytes, DWORD length) {
  DWORD written;
  return WriteFile(GetStdHandle(stream), bytes, length, &written, NULL) && written == length;
}

int wmain(int argc, wchar_t **argv) {
  if (argc != 7 || wcscmp(argv[1], L"-std=c23") != 0 || wcscmp(argv[2], L"-Wall") != 0 ||
      wcscmp(argv[3], L"-Wextra") != 0 || wcscmp(argv[5], L"-o") != 0) {
    return 9;
  }
  FILE *source;
  if (_wfopen_s(&source, argv[4], L"rb") != 0) {
    return 9;
  }
  char mode[32] = {0};
  fread(mode, 1, sizeof(mode) - 1, source);
  fclose(source);

  if (strcmp(mode, "no output") == 0) {
    return 0;
  }
  if (strcmp(mode, "large log") == 0 || strcmp(mode, "continuous log") == 0) {
    char data[4096];
    memset(data, 'a', sizeof(data));
    if (strcmp(mode, "continuous log") == 0) {
      while (emit(STD_ERROR_HANDLE, data, sizeof(data))) {
      }
      return 7;
    }
    for (size_t i = 0; i < LOG_LIMIT / sizeof(data) - 1; ++i) {
      if (!emit(STD_OUTPUT_HANDLE, data, sizeof(data))) {
        return 7;
      }
    }
    return emit(STD_ERROR_HANDLE, data, sizeof(data) - 1) &&
                   emit(STD_ERROR_HANDLE, "\xe6\x97\xa5", 3)
               ? 3
               : 7;
  }
  if (strcmp(mode, "invalid log") == 0) {
    return emit(STD_OUTPUT_HANDLE, "out", 3) && emit(STD_ERROR_HANDLE, "\xff\x00\xe6\x97\xa5", 5)
               ? 3
               : 7;
  }
  if (strcmp(mode, "success") != 0) {
    return emit(STD_ERROR_HANDLE, "error: input\n", 13) ? 3 : 7;
  }
  return CopyFileW(argv[0], argv[6], TRUE) ? 0 : 9;
}
