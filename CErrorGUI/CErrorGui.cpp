#include <windows.h>
#include <commdlg.h>
#include <string>
#include <vector>

#pragma comment(lib, "Comdlg32.lib")

#define ID_CODE 101
#define ID_RESULT 102
#define ID_RUN 103
#define ID_FILE 104
#define ID_SAVE 105
#define ID_GIT 106
#define ID_THEME 107
#define ID_COPY_RESULT 108

HWND hCode;
HWND hResult;
HFONT hFont;

// �t�H���g�ݒ�
void SetControlFont(HWND hwnd)
{
    SendMessageW(
        hwnd,
        WM_SETFONT,
        (WPARAM)hFont,
        TRUE);
}

// �t�@�C�����J��
void OpenCodeFile(HWND hwnd)
{
    wchar_t fileName[MAX_PATH] = L"";

    OPENFILENAMEW ofn = {};

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;

    ofn.lpstrFilter =
        L"C�\�[�X�t�@�C�� (*.c)\0*.c\0"
        L"���ׂẴt�@�C�� (*.*)\0*.*\0";

    ofn.nFilterIndex = 1;

    ofn.Flags =
        OFN_PATHMUSTEXIST |
        OFN_FILEMUSTEXIST;

    ofn.lpstrTitle = L"�t�@�C����I�����Ă�������";

    if (!GetOpenFileNameW(&ofn))
        return;

    HANDLE hFile = CreateFileW(
        fileName,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        MessageBoxW(
            hwnd,
            L"�t�@�C�����J���܂���ł����B",
            L"�G���[",
            MB_OK | MB_ICONERROR);

        return;
    }

    LARGE_INTEGER fileSize;

    if (!GetFileSizeEx(hFile, &fileSize) ||
        fileSize.QuadPart < 0 ||
        fileSize.QuadPart > 20 * 1024 * 1024)
    {
        CloseHandle(hFile);

        MessageBoxW(
            hwnd,
            L"�t�@�C���T�C�Y���傫�����܂��B",
            L"�G���[",
            MB_OK | MB_ICONERROR);

        return;
    }

    std::vector<char> buffer(
        static_cast<size_t>(fileSize.QuadPart));

    DWORD bytesRead = 0;

    BOOL success = TRUE;

    if (!buffer.empty())
    {
        success = ReadFile(
            hFile,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            &bytesRead,
            NULL);
    }

    CloseHandle(hFile);

    if (!success)
    {
        MessageBoxW(
            hwnd,
            L"�t�@�C���̓ǂݍ��݂Ɏ��s���܂����B",
            L"�G���[",
            MB_OK | MB_ICONERROR);

        return;
    }

    buffer.resize(bytesRead);

    if (buffer.size() >= 3 &&
        (unsigned char)buffer[0] == 0xEF &&
        (unsigned char)buffer[1] == 0xBB &&
        (unsigned char)buffer[2] == 0xBF)
    {
        buffer.erase(buffer.begin(), buffer.begin() + 3);
    }

    std::wstring code;

    if (!buffer.empty())
    {
        int size = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            buffer.data(),
            (int)buffer.size(),
            NULL,
            0);

        UINT codePage = CP_UTF8;

        if (size == 0)
        {
            codePage = CP_ACP;

            size = MultiByteToWideChar(
                codePage,
                0,
                buffer.data(),
                (int)buffer.size(),
                NULL,
                0);
        }

        if (size > 0)
        {
            code.resize(size);

            MultiByteToWideChar(
                codePage,
                0,
                buffer.data(),
                (int)buffer.size(),
                code.data(),
                size);
        }
    }

    SetWindowTextW(hCode, code.c_str());
}

// ���ʂ��R�s�[����
void CopyResult(HWND hwnd)
{
    int length = GetWindowTextLengthW(hResult);

    if (length == 0)
    {
        MessageBoxW(
            hwnd,
            L"�R�s�[���錋�ʂ�����܂���B",
            L"���m�点",
            MB_OK | MB_ICONINFORMATION);

        return;
    }

    std::wstring result(length + 1, L'\0');

    GetWindowTextW(
        hResult,
        result.data(),
        length + 1);

    result.resize(length);

    if (!OpenClipboard(hwnd))
    {
        MessageBoxW(
            hwnd,
            L"�N���b�v�{�[�h���J���܂���ł����B",
            L"�G���[",
            MB_OK | MB_ICONERROR);

        return;
    }

    EmptyClipboard();

    SIZE_T bytes =
        (result.size() + 1) * sizeof(wchar_t);

    HGLOBAL hMemory = GlobalAlloc(
        GMEM_MOVEABLE,
        bytes);

    if (!hMemory)
    {
        CloseClipboard();

        MessageBoxW(
            hwnd,
            L"�������̊m�ۂɎ��s���܂����B",
            L"�G���[",
            MB_OK | MB_ICONERROR);

        return;
    }

    wchar_t *destination =
        (wchar_t *)GlobalLock(hMemory);

    if (!destination)
    {
        GlobalFree(hMemory);
        CloseClipboard();

        return;
    }

    memcpy(
        destination,
        result.c_str(),
        bytes);

    GlobalUnlock(hMemory);

    if (!SetClipboardData(CF_UNICODETEXT, hMemory))
    {
        GlobalFree(hMemory);
        CloseClipboard();

        MessageBoxW(
            hwnd,
            L"�R�s�[�Ɏ��s���܂����B",
            L"�G���[",
            MB_OK | MB_ICONERROR);

        return;
    }

    CloseClipboard();

    MessageBoxW(
        hwnd,
        L"���ʂ��R�s�[���܂����B",
        L"����",
        MB_OK | MB_ICONINFORMATION);
}

// �E�B���h�E�̏���
LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        hFont = CreateFontW(
            18,
            0, 0, 0,
            FW_NORMAL,
            FALSE, FALSE, FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            L"Yu Gothic UI");

        // �\�[�X�R�[�h���̓��x��
        HWND hCodeLabel = CreateWindowW(
            L"STATIC",
            L"�\�[�X�R�[�h����",
            WS_CHILD | WS_VISIBLE,
            20, 75, 250, 25,
            hwnd,
            NULL,
            NULL,
            NULL);

        // ���s���ʁE�G���[���x��
        HWND hResultLabel = CreateWindowW(
            L"STATIC",
            L"���s���ʁE�G���[",
            WS_CHILD | WS_VISIBLE,
            530, 75, 250, 25,
            hwnd,
            NULL,
            NULL,
            NULL);

        // �\�[�X�R�[�h���͗�
        hCode = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE |
                WS_VSCROLL | WS_HSCROLL |
                ES_MULTILINE |
                ES_AUTOVSCROLL |
                ES_AUTOHSCROLL |
                ES_WANTRETURN,
            20, 105, 480, 400,
            hwnd,
            (HMENU)ID_CODE,
            NULL,
            NULL);

        // ���s���ʁE�G���[�\����
        hResult = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE |
                WS_VSCROLL | WS_HSCROLL |
                ES_MULTILINE |
                ES_AUTOVSCROLL |
                ES_AUTOHSCROLL |
                ES_READONLY,
            530, 105, 530, 400,
            hwnd,
            (HMENU)ID_RESULT,
            NULL,
            NULL);

        // �t�@�C�����J��
        HWND hFileButton = CreateWindowW(
            L"BUTTON",
            L"�t�@�C�����J��",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            350, 25, 150, 35,
            hwnd,
            (HMENU)ID_FILE,
            NULL,
            NULL);

        // �e�[�}�ύX
        HWND hThemeButton = CreateWindowW(
            L"BUTTON",
            L"�e�[�}�ύX",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            900, 25, 150, 35,
            hwnd,
            (HMENU)ID_THEME,
            NULL,
            NULL);

        // ���ʂ��R�s�[
        HWND hCopyButton = CreateWindowW(
            L"BUTTON",
            L"���ʂ��R�s�[",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            900, 70, 150, 30,
            hwnd,
            (HMENU)ID_COPY_RESULT,
            NULL,
            NULL);

        // ���s
        HWND hRunButton = CreateWindowW(
            L"BUTTON",
            L"���s",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            900, 520, 150, 40,
            hwnd,
            (HMENU)ID_RUN,
            NULL,
            NULL);

        // �ۑ�
        HWND hSaveButton = CreateWindowW(
            L"BUTTON",
            L"�ۑ�",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            20, 590, 150, 40,
            hwnd,
            (HMENU)ID_SAVE,
            NULL,
            NULL);

        // Git���|�W�g��
        HWND hGitButton = CreateWindowW(
            L"BUTTON",
            L"Git���|�W�g��",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            190, 590, 170, 40,
            hwnd,
            (HMENU)ID_GIT,
            NULL,
            NULL);

        // �t�H���g��ݒ�
        SetControlFont(hCodeLabel);
        SetControlFont(hResultLabel);

        SetControlFont(hCode);
        SetControlFont(hResult);

        SetControlFont(hFileButton);
        SetControlFont(hThemeButton);
        SetControlFont(hCopyButton);
        SetControlFont(hRunButton);
        SetControlFont(hSaveButton);
        SetControlFont(hGitButton);

        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case ID_FILE:
            OpenCodeFile(hwnd);
            return 0;

        case ID_COPY_RESULT:
            CopyResult(hwnd);
            return 0;

        case ID_RUN:
            MessageBoxW(
                hwnd,
                L"���s�@�\�͂܂���������Ă��܂���B",
                L"���m�点",
                MB_OK | MB_ICONINFORMATION);
            return 0;

        case ID_SAVE:
            MessageBoxW(
                hwnd,
                L"�ۑ��@�\�͂܂���������Ă��܂���B",
                L"���m�点",
                MB_OK | MB_ICONINFORMATION);
            return 0;

        case ID_GIT:
            MessageBoxW(
                hwnd,
                L"Git���|�W�g���@�\�͂܂���������Ă��܂���B",
                L"���m�点",
                MB_OK | MB_ICONINFORMATION);
            return 0;

        case ID_THEME:
            MessageBoxW(
                hwnd,
                L"�e�[�}�ύX�@�\�͂܂���������Ă��܂���B",
                L"���m�点",
                MB_OK | MB_ICONINFORMATION);
            return 0;
        }

        break;
    }

    case WM_DESTROY:

        if (hFont)
        {
            DeleteObject(hFont);
            hFont = NULL;
        }

        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam);
}

// �A�v���P�[�V�����̊J�n
int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR pCmdLine,
    int nCmdShow)
{
    const wchar_t CLASS_NAME[] =
        L"CErrorExplanationTool";

    WNDCLASSW wc = {};

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground =
        (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassW(&wc))
        return 0;

    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"C����G���[����c�[��",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1100,
        700,
        NULL,
        NULL,
        hInstance,
        NULL);

    if (!hwnd)
        return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};

    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
