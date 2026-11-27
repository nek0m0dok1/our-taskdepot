#include "../resources/resource.h"

#include <windows.h>

static INT_PTR CALLBACK main_dialog_proc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam) {
  (void)lparam;

  if (message == WM_INITDIALOG) {
    return TRUE;
  }

  if (message == WM_CLOSE || (message == WM_COMMAND && LOWORD(wparam) == IDCANCEL)) {
    EndDialog(dialog, 0);
    return TRUE;
  }

  return FALSE;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): Win32 defines this signature.
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous_instance, PWSTR command_line,
                    int show_command) {
  (void)previous_instance;
  (void)command_line;
  (void)show_command;

  return DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_MAIN), NULL, main_dialog_proc, 0) == -1;
}
