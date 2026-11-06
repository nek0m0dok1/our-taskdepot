#include <windows.h>
#pragma comment(lib, "user32.lib")
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) { // 1. Main Window Class သတ်မှတ်ခြင်း WNDCLASS wc = {0}; wc.lpfnWndProc = DefWindowProc; wc.hInstance = hInstance; wc.lpszClassName = "TaskDepotClass"; wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
C

RegisterClass(&wc);


HWND hwnd = CreateWindowEx(
    0, "TaskDepotClass", "our-taskdepot GUI Playground",
    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
    100, 100, 800, 500,
    NULL, NULL, hInstance, NULL
);

// 3. Input Area (ဘယ်ဘက်)
CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "// Write C Code Here...",
    WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE,
    10, 10, 370, 400, hwnd, (HMENU)101, NULL, NULL);

// 4. Output Area (ညာဘက်)
CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "--- Output Window ---",
    WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
    390, 10, 380, 400, hwnd, (HMENU)102, NULL, NULL);

// 5. Run Button
CreateWindow("BUTTON", "Run Code",
    WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
    10, 420, 100, 30, hwnd, (HMENU)103, NULL, NULL);

// Message Loop
MSG msg = {0};
while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
}

return 0;
}
#include <windows.h>
#pragma comment(lib, "user32.lib")
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) { // 1. Main Window Class WNDCLASSA wc = {0};
C

wc.lpfnWndProc = DefWindowProcA;
wc.hInstance = hInstance;
wc.lpszClassName = "TaskDepotClass";
wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

RegisterClassA(&wc);

// 2. Main Window
HWND hwnd = CreateWindowExA(
    0,
    "TaskDepotClass",
    "our-taskdepot GUI Playground",
    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
    100, 100, 800, 500,
    NULL, NULL, hInstance, NULL
);

// 3. Input Area
CreateWindowExA(
    WS_EX_CLIENTEDGE, "EDIT", "// Write C Code Here...",
    WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE,
    10, 10, 370, 400,
    hwnd, (HMENU)101, hInstance, NULL
);

// 4. Output Area
CreateWindowExA(
    WS_EX_CLIENTEDGE, "EDIT", "--- Output Window ---",
    WS_CHILD | WS_VISIBLE | WS_VSCROLL |
    ES_MULTILINE | ES_READONLY,
    390, 10, 380, 400,
    hwnd, (HMENU)102, hInstance, NULL
);

// 5. Run Button
CreateWindowA(
    "BUTTON", "Run Code",
    WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
    10, 420, 100, 30,
    hwnd, (HMENU)103, hInstance, NULL
);

// 6. Message Loop
MSG msg = {0};

while (GetMessageA(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
}

return 0;
}
