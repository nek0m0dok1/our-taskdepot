#include "mock_backend.h"
#include <windows.h>
int mock_compile(const char *code);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {

  case WM_COMMAND:
    if (LOWORD(wParam) == 101) {
      HWND hEdit = GetDlgItem(hwnd, 201);
      char code[10000];

      GetWindowTextA(hEdit, code, sizeof(code));

      MockResult result = mock_compile_result(code);
      switch (result.status) {
      case MOCK_COMPILING:
        MessageBoxW(hwnd, L"\u30b3\u30f3\u30d1\u30a4\u30eb\u4e2d\u3067\u3059\u3002", L"Compiling",
                    MB_OK);
        break;

      case MOCK_COMPILE_SUCCESS:
        MessageBoxW(
            hwnd, L"\u30b3\u30f3\u30d1\u30a4\u30eb\u306b\u6210\u529f\u3057\u307e\u3057\u305f\u3002",
            L"Compile", MB_OK);
        break;

      case MOCK_COMPILE_ERROR:
        MessageBoxW(hwnd,
                    L"\u30b3\u30f3\u30d1\u30a4\u30eb\u30a8\u30e9\u30fc\u304c\u767a\u751f\u3057"
                    L"\u307e\u3057\u305f\u3002",
                    L"Compile Error", MB_OK);
        break;

      case MOCK_MULTIPLE_ERRORS:
        MessageBoxW(hwnd,
                    L"\u8907\u6570\u306e\u30a8\u30e9\u30fc\u304c\u767a\u751f\u3057\u307e\u3057"
                    L"\u305f\u3002\n\u6b8b\u308a\u306e\u30a8\u30e9\u30fc: 2",
                    L"Multiple Errors", MB_OK);
        break;

      case MOCK_UNKNOWN_ERROR:
        MessageBoxW(
            hwnd,
            L"\u4e0d\u660e\u306a\u30a8\u30e9\u30fc\u304c\u767a\u751f\u3057\u307e\u3057\u305f\u3002",
            L"Unknown Error", MB_OK);
        break;

      default:
        MessageBoxW(hwnd, L"Unknown Status!", L"Result", MB_OK);
        break;
      }

      return 0;
    }
    if (LOWORD(wParam) == 102) {
      HWND hEdit = GetDlgItem(hwnd, 201);
      char code[10000] = {0};

      GetWindowTextA(hEdit, code, sizeof(code));

      MockResult result = mock_run_result(code);
      char details[2048];

      sprintf_s(details, sizeof(details), "stdout: %s\nstderr: %s\nexit status: %d",
                result.stdout_text ? result.stdout_text : "",
                result.stderr_text ? result.stderr_text : "", result.exit_status);

      MessageBoxA(hwnd, details, "Run Details", MB_OK);

      if (result.status == MOCK_RUN_ERROR) {
        MessageBoxW(hwnd,
                    L"\u30d7\u30ed\u30b0\u30e9\u30e0\u306e\u5b9f\u884c\u306b\u5931\u6557\u3057"
                    L"\u307e\u3057\u305f\u3002",
                    L"Run Result", MB_OK);
      } else {
        MessageBoxW(hwnd,
                    L"\u30d7\u30ed\u30b0\u30e9\u30e0\u306e\u5b9f\u884c\u306b\u6210\u529f\u3057"
                    L"\u307e\u3057\u305f\u3002",
                    L"Run Result", MB_OK);
      }

      return 0;
    }
    break;

  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }

  return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  WNDCLASSA wc = {0};

  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = "TaskDepotWindow";
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

  if (!RegisterClassA(&wc)) {
    return 1;
  }

  HWND hwnd = CreateWindowExA(0, "TaskDepotWindow", "our-taskdepot", WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, 800, 500, NULL, NULL, hInstance, NULL);
  HWND hEditor = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                 WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                                     ES_AUTOVSCROLL | ES_WANTRETURN,
                                 20, 20, 740, 300, hwnd, (HMENU)201, hInstance, NULL);

  HWND hCompile = CreateWindowExA(0, "BUTTON", "Compile", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20,
                                  350, 120, 40, hwnd, (HMENU)101, hInstance, NULL);

  HWND hRun = CreateWindowExA(0, "BUTTON", "Run", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 160, 350,
                              120, 40, hwnd, (HMENU)102, hInstance, NULL);

  if (hwnd == NULL) {
    return 1;
  }

  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);

  MSG msg;
  while (GetMessageA(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }

  return (int)msg.wParam;
}
