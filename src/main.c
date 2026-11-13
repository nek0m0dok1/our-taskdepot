#define UNICODE
#define _UNICODE

#include "mock_backend.h"
#include <commdlg.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")

#define ID_EDITOR 201
#define ID_RESULT 202
#define ID_RUN 101
#define ID_SAVE 103
#define ID_GIT 104
#define ID_FONT 105
#define ID_THEME 106
#define ID_TAB_RESULT 107
#define ID_TAB_ERROR 108
#define ID_TAB_FILE 109
#define ID_TAB_ADD 110

static HWND hMain, hEditor, hResult;
static HWND hFont, hTheme;
static HWND hRun, hSave, hGit;
static HWND hTabResult, hTabError, hTabFile, hTabAdd;
static HWND hLabelCode, hLabelResult, hLabelFont, hLabelTheme;

static HFONT hUIFont, hCodeFont;
static HBRUSH hBackground, hEditBackground;

static int fontSize = 14;
static int themeMode = 0;
static int currentTab = 0;

static wchar_t resultText[8192] = L"";
static wchar_t errorText[8192] = L"";

static COLORREF bgColor, textColor, editColor;

static void Utf8ToWide(const char *src, wchar_t *dst, int capacity) {
  if (!dst || capacity <= 0)
    return;
  dst[0] = L'\0';
  if (!src)
    return;

  if (!MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, capacity))
    dst[0] = L'\0';

  dst[capacity - 1] = L'\0';
}

static char *WideToUtf8Alloc(const wchar_t *src) {
  if (!src)
    return NULL;

  int bytes = WideCharToMultiByte(CP_UTF8, 0, src, -1, NULL, 0, NULL, NULL);

  if (bytes <= 0)
    return NULL;

  char *dst = (char *)malloc(bytes);
  if (!dst)
    return NULL;

  if (!WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, bytes, NULL, NULL)) {
    free(dst);
    return NULL;
  }

  return dst;
}

static HWND AddControl(const wchar_t *type, const wchar_t *text, DWORD style, int id) {
  return CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 100, 30, hMain,
                         (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
}

static void Move(HWND h, int x, int y, int w, int height) {
  if (!h)
    return;

  if (w < 1)
    w = 1;
  if (height < 1)
    height = 1;

  MoveWindow(h, x, y, w, height, TRUE);
}

/* RESPONSIVE LAYOUT */

static void UpdateLayout(void) {
  if (!hMain)
    return;

  RECT rc;
  GetClientRect(hMain, &rc);

  int W = rc.right - rc.left;
  int H = rc.bottom - rc.top;

  int margin = 18;
  int gap = 16;

  int contentW = W - margin * 2;
  int columnW = (contentW - gap) / 2;

  int leftX = margin;
  int rightX = margin + columnW + gap;

  int topY = 15;

  Move(hLabelFont, leftX, topY + 5, 115, 30);
  Move(hFont, leftX + 120, topY, 110, 200);

  Move(hLabelTheme, rightX, topY + 5, 110, 30);
  Move(hTheme, rightX + 115, topY, columnW - 115, 200);

  int labelY = 75;

  Move(hLabelCode, leftX, labelY, columnW, 30);
  Move(hLabelResult, rightX, labelY, columnW, 30);

  int tabY = 112;

  Move(hTabFile, leftX, tabY, 110, 32);
  Move(hTabAdd, leftX + 115, tabY, 40, 32);

  Move(hTabResult, rightX, tabY, 100, 32);
  Move(hTabError, rightX + 110, tabY, 100, 32);

  int editorY = 150;

  int bottomButtonY = H - 58;

  int editorHeight = bottomButtonY - editorY - 12;

  if (editorHeight < 60)
    editorHeight = 60;

  Move(hEditor, leftX, editorY, columnW, editorHeight);

  Move(hResult, rightX, editorY, columnW, editorHeight);

  int buttonHeight = 38;

  Move(hSave, leftX, bottomButtonY, 110, buttonHeight);

  Move(hGit, leftX + 120, bottomButtonY, 155, buttonHeight);

  Move(hRun, rightX + columnW - 130, bottomButtonY, 130, buttonHeight);
}

/* FONT */

static void UpdateFonts(void) {
  HFONT newUI =
      CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                  CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Yu Gothic UI");

  HFONT newCode = CreateFontW(-MulDiv(fontSize, 96, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");

  HWND controls[] = {hRun,        hSave,   hGit,       hTabResult,   hTabError,
                     hTabFile,    hTabAdd, hLabelCode, hLabelResult, hLabelFont,
                     hLabelTheme, hFont,   hTheme};

  int count = sizeof(controls) / sizeof(controls[0]);

  for (int i = 0; i < count; i++) {
    if (controls[i])
      SendMessageW(controls[i], WM_SETFONT, (WPARAM)newUI, TRUE);
  }

  if (hEditor)
    SendMessageW(hEditor, WM_SETFONT, (WPARAM)newCode, TRUE);

  if (hResult)
    SendMessageW(hResult, WM_SETFONT, (WPARAM)newCode, TRUE);

  if (hUIFont)
    DeleteObject(hUIFont);
  if (hCodeFont)
    DeleteObject(hCodeFont);

  hUIFont = newUI;
  hCodeFont = newCode;
}

/* THEME */

static void ApplyTheme(void) {
  if (themeMode == 1) {
    bgColor = RGB(35, 39, 47);
    editColor = RGB(25, 28, 34);
    textColor = RGB(235, 235, 235);
  } else if (themeMode == 2) {
    bgColor = RGB(230, 240, 255);
    editColor = RGB(250, 252, 255);
    textColor = RGB(20, 40, 80);
  } else {
    bgColor = RGB(245, 248, 252);
    editColor = RGB(255, 255, 255);
    textColor = RGB(20, 20, 20);
  }

  HBRUSH newBackground = CreateSolidBrush(bgColor);
  HBRUSH newEditBackground = CreateSolidBrush(editColor);

  HBRUSH oldBackground = hBackground;
  HBRUSH oldEditBackground = hEditBackground;

  hBackground = newBackground;
  hEditBackground = newEditBackground;

  if (hMain)
    RedrawWindow(hMain, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);

  if (oldBackground)
    DeleteObject(oldBackground);

  if (oldEditBackground)
    DeleteObject(oldEditBackground);
}

/* RESULT */

static void ShowResultTab(int tab) {
  currentTab = tab;

  SetWindowTextW(hResult, tab == 0 ? resultText : errorText);
}

/* GET CODE */

static char *GetEditorCode(void) {
  int length = GetWindowTextLengthW(hEditor);

  wchar_t *wide = (wchar_t *)calloc((size_t)length + 1, sizeof(wchar_t));

  if (!wide)
    return NULL;

  GetWindowTextW(hEditor, wide, length + 1);

  char *code = WideToUtf8Alloc(wide);

  free(wide);
  return code;
}

/* RUN */

static void RunCode(void) {
  char *code = GetEditorCode();

  if (!code) {
    MessageBoxW(
        hMain,
        L"\u30e1\u30e2\u30ea\u306e\u78ba\u4fdd\u306b\u5931\u6557\u3057\u307e\u3057\u305f\u3002",
        L"\u30a8\u30e9\u30fc", MB_OK | MB_ICONERROR);
    return;
  }

  resultText[0] = L'\0';
  errorText[0] = L'\0';

  MockResult compile = mock_compile_result(code);

  if (compile.status != MOCK_COMPILE_SUCCESS) {
    wchar_t message[2048] = L"";
    wchar_t original[2048] = L"";

    Utf8ToWide(compile.japanese_message, message, 2048);

    Utf8ToWide(compile.original_message, original, 2048);

    if (compile.status == MOCK_COMPILING) {
      wcscpy_s(resultText, 8192, message);
      ShowResultTab(0);
    } else {
      swprintf_s(errorText, 8192,
                 L"%ls\r\n\r\n%ls\r\n\r\n"
                 L"\u30a8\u30e9\u30fc\u6570: %d",
                 message, original, compile.remaining_errors);
      ShowResultTab(1);
    }

    free(code);
    return;
  }

  MockResult run = mock_run_result(code);

  wchar_t output[4096] = L"";
  wchar_t errors[4096] = L"";
  wchar_t message[2048] = L"";

  Utf8ToWide(run.stdout_text, output, 4096);
  Utf8ToWide(run.stderr_text, errors, 4096);
  Utf8ToWide(run.japanese_message, message, 2048);

  if (run.status == MOCK_RUN_SUCCESS) {
    swprintf_s(resultText, 8192,
               L"%ls\r\n\r\n"
               L"\u7d42\u4e86\u30b3\u30fc\u30c9: %d",
               output, run.exit_status);
    ShowResultTab(0);
  } else {
    swprintf_s(errorText, 8192,
               L"%ls\r\n\r\n%ls\r\n\r\n"
               L"\u7d42\u4e86\u30b3\u30fc\u30c9: %d",
               message, errors, run.exit_status);

    ShowResultTab(1);
  }

  free(code);
}

/* SAVE */

static void SaveCode(void) {
  wchar_t filename[MAX_PATH] = L"main.c";

  OPENFILENAMEW ofn = {0};

  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hMain;
  ofn.lpstrFile = filename;
  ofn.nMaxFile = MAX_PATH;

  ofn.lpstrFilter = L"C Files (.c)\0.c\0"
                    L"All Files (.)\0*.*\0";

  ofn.lpstrDefExt = L"c";

  ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

  if (!GetSaveFileNameW(&ofn))
    return;

  char *code = GetEditorCode();

  if (!code)
    return;

  FILE *fp = NULL;

  if (_wfopen_s(&fp, filename, L"wb") == 0 && fp) {
    size_t length = strlen(code);
    size_t written = fwrite(code, 1, length, fp);
    int closed = fclose(fp);

    if (written == length && closed == 0) {
      MessageBoxW(hMain,
                  L"\u30d5\u30a1\u30a4\u30eb\u3092\u4fdd\u5b58\u3057\u307e\u3057\u305f\u3002",
                  L"\u4fdd\u5b58\u5b8c\u4e86", MB_OK | MB_ICONINFORMATION);
    } else {
      MessageBoxW(hMain, L"\u4fdd\u5b58\u306b\u5931\u6557\u3057\u307e\u3057\u305f\u3002",
                  L"\u30a8\u30e9\u30fc", MB_OK | MB_ICONERROR);
    }
  } else {
    MessageBoxW(hMain, L"\u4fdd\u5b58\u306b\u5931\u6557\u3057\u307e\u3057\u305f\u3002",
                L"\u30a8\u30e9\u30fc", MB_OK | MB_ICONERROR);
  }

  free(code);
}

/* GIT */

static void OpenGitRepository(void) {
  HINSTANCE result = ShellExecuteW(hMain, L"open", L"explorer.exe", L".", NULL, SW_SHOWNORMAL);

  if ((INT_PTR)result <= 32) {
    MessageBoxW(hMain, L"\u30d5\u30a9\u30eb\u30c0\u30fc\u3092\u958b\u3051\u307e\u305b\u3093\u3002",
                L"\u30a8\u30e9\u30fc", MB_OK | MB_ICONERROR);
  }
}

/* WINDOW PROCEDURE */

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {

  case WM_CREATE: {
    hMain = hwnd;

    hLabelFont = AddControl(L"STATIC", L"\u30d5\u30a9\u30f3\u30c8\u30b5\u30a4\u30ba\uff1a", 0, 0);

    hFont = AddControl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, ID_FONT);

    const wchar_t *sizes[] = {L"12", L"14", L"16", L"18", L"20"};

    for (int i = 0; i < 5; i++)
      SendMessageW(hFont, CB_ADDSTRING, 0, (LPARAM)sizes[i]);

    SendMessageW(hFont, CB_SETCURSEL, 1, 0);

    hLabelTheme = AddControl(L"STATIC", L"\u30c6\u30fc\u30de\u5909\u66f4\uff1a", 0, 0);

    hTheme = AddControl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, ID_THEME);

    SendMessageW(hTheme, CB_ADDSTRING, 0, (LPARAM)L"\u30e9\u30a4\u30c8");

    SendMessageW(hTheme, CB_ADDSTRING, 0, (LPARAM)L"\u30c0\u30fc\u30af");

    SendMessageW(hTheme, CB_ADDSTRING, 0, (LPARAM)L"\u30ab\u30b9\u30bf\u30e0");

    SendMessageW(hTheme, CB_SETCURSEL, 0, 0);

    hLabelCode = AddControl(L"STATIC", L"\u30b3\u30fc\u30c9\u5165\u529b", 0, 0);

    hLabelResult = AddControl(L"STATIC", L"\u5b9f\u884c\u7d50\u679c", 0, 0);

    hTabFile = AddControl(L"BUTTON", L"main.c", BS_PUSHBUTTON, ID_TAB_FILE);

    hTabAdd = AddControl(L"BUTTON", L"+", BS_PUSHBUTTON, ID_TAB_ADD);

    hTabResult = AddControl(L"BUTTON", L"\u7d50\u679c", BS_PUSHBUTTON, ID_TAB_RESULT);

    hTabError = AddControl(L"BUTTON", L"\u30a8\u30e9\u30fc", BS_PUSHBUTTON, ID_TAB_ERROR);

    hEditor = AddControl(L"EDIT", L"",
                         WS_BORDER | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL |
                             ES_AUTOHSCROLL | ES_WANTRETURN,
                         ID_EDITOR);

    SendMessageW(hEditor, EM_SETLIMITTEXT, 100000, 0);

    hResult =
        AddControl(L"EDIT", L"",
                   WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, ID_RESULT);

    hRun = AddControl(L"BUTTON", L"\u5b9f\u884c", BS_PUSHBUTTON, ID_RUN);

    hSave = AddControl(L"BUTTON", L"\u4fdd\u5b58", BS_PUSHBUTTON, ID_SAVE);

    hGit = AddControl(L"BUTTON", L"Git \u30ea\u30dd\u30b8\u30c8\u30ea", BS_PUSHBUTTON, ID_GIT);

    UpdateFonts();
    ApplyTheme();
    UpdateLayout();

    return 0;
  }

  case WM_SIZE:
    UpdateLayout();
    return 0;

  case WM_COMMAND: {
    int id = LOWORD(wParam);
    int notification = HIWORD(wParam);

    if (id == ID_RUN && notification == BN_CLICKED)
      RunCode();

    else if (id == ID_SAVE && notification == BN_CLICKED)
      SaveCode();

    else if (id == ID_GIT && notification == BN_CLICKED)
      OpenGitRepository();

    else if (id == ID_TAB_RESULT && notification == BN_CLICKED)
      ShowResultTab(0);

    else if (id == ID_TAB_ERROR && notification == BN_CLICKED)
      ShowResultTab(1);

    else if (id == ID_TAB_ADD && notification == BN_CLICKED)
      MessageBoxW(hwnd,
                  L"\u65b0\u3057\u3044\u30bf\u30d6\u306e\u8ffd\u52a0\u306f\u6e96\u5099\u4e2d\u3067"
                  L"\u3059\u3002",
                  L"\u304a\u77e5\u3089\u305b", MB_OK | MB_ICONINFORMATION);

    else if (id == ID_FONT && notification == CBN_SELCHANGE) {
      int index = (int)SendMessageW(hFont, CB_GETCURSEL, 0, 0);

      int sizes[] = {12, 14, 16, 18, 20};

      if (index >= 0 && index < 5) {
        fontSize = sizes[index];
        UpdateFonts();
      }
    }

    else if (id == ID_THEME && notification == CBN_SELCHANGE) {
      themeMode = (int)SendMessageW(hTheme, CB_GETCURSEL, 0, 0);

      ApplyTheme();
    }

    return 0;
  }

  case WM_CTLCOLOREDIT:
  case WM_CTLCOLORSTATIC: {
    HDC hdc = (HDC)wParam;
    HWND control = (HWND)lParam;

    SetTextColor(hdc, textColor);

    if (control == hEditor || control == hResult) {
      SetBkColor(hdc, editColor);
      return (LRESULT)hEditBackground;
    }

    SetBkColor(hdc, bgColor);
    return (LRESULT)hBackground;
  }

  case WM_ERASEBKGND: {
    if (!hBackground)
      break;

    RECT rect;
    GetClientRect(hwnd, &rect);

    FillRect((HDC)wParam, &rect, hBackground);

    return 1;
  }

  case WM_GETMINMAXINFO: {
    MINMAXINFO *info = (MINMAXINFO *)lParam;

    info->ptMinTrackSize.x = 620;
    info->ptMinTrackSize.y = 390;

    return 0;
  }

  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

/* MAIN */

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  (void)hPrevInstance;
  (void)lpCmdLine;

  SetProcessDPIAware();

  WNDCLASSW wc = {0};

  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = L"TaskDepotWindow";
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.hCursor = LoadCursorW(NULL, IDC_ARROW);

  if (!RegisterClassW(&wc))
    return 1;

  RECT workArea;

  SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);

  int availableW = workArea.right - workArea.left;
  int availableH = workArea.bottom - workArea.top;

  int windowW = availableW - 80;
  int windowH = availableH - 80;

  if (windowW > 1100)
    windowW = 1100;
  if (windowH > 750)
    windowH = 750;

  if (windowW < 620)
    windowW = availableW;
  if (windowH < 390)
    windowH = availableH;

  int x = workArea.left + (availableW - windowW) / 2;
  int y = workArea.top + (availableH - windowH) / 2;

  HWND hwnd = CreateWindowExW(0, L"TaskDepotWindow", L"our-taskdepot", WS_OVERLAPPEDWINDOW, x, y,
                              windowW, windowH, NULL, NULL, hInstance, NULL);

  if (!hwnd)
    return 1;

  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);

  MSG msg;

  while (GetMessageW(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  if (hUIFont)
    DeleteObject(hUIFont);
  if (hCodeFont)
    DeleteObject(hCodeFont);
  if (hBackground)
    DeleteObject(hBackground);
  if (hEditBackground)
    DeleteObject(hEditBackground);

  return (int)msg.wParam;
}
