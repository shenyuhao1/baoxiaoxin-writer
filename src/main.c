#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#define _WIN32_IE 0x0600

#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "resource.h"
#include "config.h"
#include "ui.h"
#include "worker.h"

// ── 全局状态 ─────────────────────────────────────────────
static AppUI     g_ui;
static AppConfig g_cfg;
static wchar_t   g_iniPath[MAX_PATH];
static int       g_state = STATE_IDLE;

static WorkerParams *g_worker = NULL;
static HANDLE        g_hThread = NULL;

// 防止 Trackbar ↔ EditInterval 互相触发
static BOOL g_syncingInterval = FALSE;

// ── 辅助函数 ─────────────────────────────────────────────

static int GetCurrentDelay(void)
{
    return (int)SendMessageW(g_ui.hwndTrackbar, TBM_GETPOS, 0, 0);
}

static void SetState(int newState)
{
    g_state = newState;
    UI_SetState(&g_ui, newState);

    switch (newState) {
    case STATE_IDLE:
        UI_SetStatus(&g_ui, L"就绪");
        break;
    case STATE_READY:
        UI_SetStatus(&g_ui, L"请将光标定位到目标输入框，然后点击\"开始输入\"");
        break;
    case STATE_RUNNING:
        UI_SetStatus(&g_ui, L"正在输入…");
        break;
    case STATE_PAUSED:
        UI_SetStatus(&g_ui, L"已暂停");
        break;
    }
}

// 加载文件并检测编码（UTF-8 BOM / UTF-16 LE / UTF-16 BE / 无BOM默认UTF-8）
static BOOL LoadTextFile(const wchar_t *path)
{
    HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxW(g_ui.hwndMain, L"无法打开文件。", L"错误", MB_ICONERROR);
        return FALSE;
    }

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == 0 || fileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return FALSE;
    }

    BYTE *raw = (BYTE *)HeapAlloc(GetProcessHeap(), 0, fileSize + 4);
    if (!raw) { CloseHandle(hFile); return FALSE; }

    DWORD bytesRead = 0;
    ReadFile(hFile, raw, fileSize, &bytesRead, NULL);
    CloseHandle(hFile);
    raw[bytesRead] = raw[bytesRead+1] = raw[bytesRead+2] = 0;

    wchar_t *wtext = NULL;
    int wlen = 0;

    if (bytesRead >= 2 && raw[0] == 0xFF && raw[1] == 0xFE) {
        // UTF-16 LE
        wtext = (wchar_t *)(raw + 2);
        wlen  = (int)((bytesRead - 2) / sizeof(wchar_t));
        wchar_t *copy = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, (wlen + 1) * sizeof(wchar_t));
        if (copy) {
            memcpy(copy, wtext, wlen * sizeof(wchar_t));
            copy[wlen] = 0;
            SetWindowTextW(g_ui.hwndEditText, copy);
            HeapFree(GetProcessHeap(), 0, copy);
        }
    } else if (bytesRead >= 2 && raw[0] == 0xFE && raw[1] == 0xFF) {
        // UTF-16 BE — 字节交换
        int chars = (int)((bytesRead - 2) / 2);
        wchar_t *copy = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, (chars + 1) * sizeof(wchar_t));
        if (copy) {
            BYTE *src = raw + 2;
            for (int i = 0; i < chars; ++i) {
                copy[i] = (wchar_t)((src[i*2] << 8) | src[i*2+1]);
            }
            copy[chars] = 0;
            SetWindowTextW(g_ui.hwndEditText, copy);
            HeapFree(GetProcessHeap(), 0, copy);
        }
    } else {
        // UTF-8（有无BOM均可）
        BYTE *utf8 = raw;
        DWORD utf8Len = bytesRead;
        if (bytesRead >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF) {
            utf8    = raw + 3;
            utf8Len = bytesRead - 3;
        }
        wlen = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)utf8, (int)utf8Len, NULL, 0);
        wchar_t *copy = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, (wlen + 1) * sizeof(wchar_t));
        if (copy) {
            MultiByteToWideChar(CP_UTF8, 0, (LPCCH)utf8, (int)utf8Len, copy, wlen);
            copy[wlen] = 0;
            SetWindowTextW(g_ui.hwndEditText, copy);
            HeapFree(GetProcessHeap(), 0, copy);
        }
    }

    HeapFree(GetProcessHeap(), 0, raw);
    return TRUE;
}

static void OnBtnLoad(void)
{
    OPENFILENAMEW ofn;
    wchar_t filePath[MAX_PATH] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = g_ui.hwndMain;
    ofn.lpstrFilter = L"文本文件\0*.txt;*.md;*.csv\0所有文件\0*.*\0";
    ofn.lpstrFile   = filePath;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrInitialDir = g_cfg.lastDir[0] ? g_cfg.lastDir : NULL;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        LoadTextFile(filePath);
        // 保存目录
        wcsncpy(g_cfg.lastDir, filePath, MAX_PATH - 1);
        g_cfg.lastDir[MAX_PATH - 1] = 0;
        wchar_t *slash = wcsrchr(g_cfg.lastDir, L'\\');
        if (slash) *(slash + 1) = 0;
    }
}

static void OnBtnPrepare(void)
{
    int textLen = GetWindowTextLengthW(g_ui.hwndEditText);
    if (textLen == 0) {
        MessageBoxW(g_ui.hwndMain,
            L"请先在文本框中输入或加载文字。",
            L"提示", MB_ICONINFORMATION);
        return;
    }
    SetState(STATE_READY);
    MessageBoxW(g_ui.hwndMain,
        L"请将鼠标光标点击到目标输入框（如浏览器输入框），\n"
        L"然后回到本程序点击\"开始输入\"。\n\n"
        L"也可以使用全局热键 Ctrl+Alt+V 直接开始。",
        L"准备输入", MB_ICONINFORMATION);
}

static void StartTyping(void)
{
    if (g_state != STATE_READY) return;

    int textLen = GetWindowTextLengthW(g_ui.hwndEditText);
    if (textLen == 0) {
        SetState(STATE_IDLE);
        return;
    }

    wchar_t *buf = (wchar_t *)HeapAlloc(GetProcessHeap(), 0,
                                         (textLen + 1) * sizeof(wchar_t));
    if (!buf) return;
    GetWindowTextW(g_ui.hwndEditText, buf, textLen + 1);

    g_worker = (WorkerParams *)HeapAlloc(GetProcessHeap(),
                                          HEAP_ZERO_MEMORY, sizeof(WorkerParams));
    if (!g_worker) { HeapFree(GetProcessHeap(), 0, buf); return; }

    g_worker->text      = buf;
    g_worker->textLen   = textLen;
    g_worker->delayMs   = GetCurrentDelay();
    g_worker->hwndMain  = g_ui.hwndMain;

    g_hThread = Worker_Start(g_worker);
    if (!g_hThread) {
        Worker_Free(g_worker);
        g_worker = NULL;
        return;
    }

    UI_UpdateProgress(&g_ui, 0, textLen);
    SetState(STATE_RUNNING);
}

static void OnBtnPause(void)
{
    if (g_state == STATE_RUNNING) {
        Worker_Pause(g_worker);
        SetState(STATE_PAUSED);
    } else if (g_state == STATE_PAUSED) {
        Worker_Resume(g_worker);
        SetState(STATE_RUNNING);
    }
}

static void OnBtnStop(void)
{
    if (!g_worker) return;
    Worker_Stop(g_worker);
    // 等待线程退出（最多3秒）
    if (g_hThread) {
        WaitForSingleObject(g_hThread, 3000);
        CloseHandle(g_hThread);
        g_hThread = NULL;
    }
    Worker_Free(g_worker);
    g_worker = NULL;
    SetState(STATE_IDLE);
    UI_UpdateProgress(&g_ui, 0, 0);
}

static void OnWorkerDone(BOOL stopped)
{
    if (g_hThread) {
        CloseHandle(g_hThread);
        g_hThread = NULL;
    }
    if (g_worker) {
        Worker_Free(g_worker);
        g_worker = NULL;
    }

    SetState(STATE_IDLE);

    if (!stopped) {
        UI_SetStatus(&g_ui, L"输入完成！");
        MessageBeep(MB_OK);
    }
}

static void OnTrackbarChange(void)
{
    if (g_syncingInterval) return;
    g_syncingInterval = TRUE;
    int pos = GetCurrentDelay();
    UI_SetIntervalText(&g_ui, pos);
    g_cfg.delayMs = pos;
    g_syncingInterval = FALSE;
}

static void OnIntervalEditChange(void)
{
    if (g_syncingInterval) return;
    g_syncingInterval = TRUE;
    wchar_t buf[16];
    GetWindowTextW(g_ui.hwndEditInterval, buf, 16);
    int val = _wtoi(buf);
    if (val >= INTERVAL_MIN && val <= INTERVAL_MAX) {
        SendMessageW(g_ui.hwndTrackbar, TBM_SETPOS, TRUE, val);
        g_cfg.delayMs = val;
    }
    g_syncingInterval = FALSE;
}

static void OnPresetChange(void)
{
    int sel = (int)SendMessageW(g_ui.hwndComboPreset, CB_GETCURSEL, 0, 0);
    int delay = INTERVAL_DEF;
    if (sel == 0) delay = SPEED_SLOW;
    else if (sel == 1) delay = SPEED_MEDIUM;
    else if (sel == 2) delay = SPEED_FAST;

    g_syncingInterval = TRUE;
    SendMessageW(g_ui.hwndTrackbar, TBM_SETPOS, TRUE, delay);
    UI_SetIntervalText(&g_ui, delay);
    g_cfg.delayMs = delay;
    g_syncingInterval = FALSE;
}

// ── 窗口过程 ─────────────────────────────────────────────

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        UI_Create(hwnd, &g_ui);
        UI_SetIntervalText(&g_ui, g_cfg.delayMs);
        SendMessageW(g_ui.hwndTrackbar, TBM_SETPOS, TRUE, g_cfg.delayMs);
        UI_SetPresetSelection(&g_ui, g_cfg.delayMs);
        RegisterHotKey(hwnd, HOTKEY_START, MOD_CONTROL | MOD_ALT, 'V');
        SendMessageW(g_ui.hwndChkTopmost, BM_SETCHECK,
            g_cfg.alwaysOnTop ? BST_CHECKED : BST_UNCHECKED, 0);
        if (g_cfg.alwaysOnTop)
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        return 0;

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, g_ui.hbrBg);
        return 1;
    }

    case WM_DRAWITEM:
        return UI_OnDrawItem(&g_ui, wParam, lParam);

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
        return UI_OnCtlColor(&g_ui, hwnd, msg, wParam, lParam);

    case WM_SIZE:
        UI_Layout(&g_ui, LOWORD(lParam), HIWORD(lParam));
        UI_ApplyWindowStyling(hwnd, g_cfg.darkMode);
        return 0;

    case WM_HOTKEY:
        if (wParam == HOTKEY_START) {
            if (g_state == STATE_IDLE) {
                OnBtnPrepare();
            } else if (g_state == STATE_READY) {
                StartTyping();
            }
        }
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if (id == IDC_BTN_LOAD)    { OnBtnLoad();    return 0; }
        if (id == IDC_BTN_PREPARE) { OnBtnPrepare(); return 0; }
        if (id == IDC_BTN_START)   { StartTyping();  return 0; }
        if (id == IDC_BTN_PAUSE)   { OnBtnPause();   return 0; }
        if (id == IDC_BTN_STOP)    { OnBtnStop();    return 0; }
        if (id == IDC_CHK_TOPMOST && code == BN_CLICKED) {
            BOOL checked = (SendMessageW(g_ui.hwndChkTopmost, BM_GETCHECK, 0, 0) == BST_CHECKED);
            g_cfg.alwaysOnTop = checked;
            SetWindowPos(hwnd,
                checked ? HWND_TOPMOST : HWND_NOTOPMOST,
                0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            return 0;
        }

        if (id == IDC_COMBO_PRESET && code == CBN_SELCHANGE) {
            OnPresetChange();
            return 0;
        }
        if (id == IDC_EDIT_INTERVAL && code == EN_CHANGE) {
            OnIntervalEditChange();
            return 0;
        }
        break;
    }

    case WM_HSCROLL:
        if ((HWND)lParam == g_ui.hwndTrackbar) {
            OnTrackbarChange();
        }
        return 0;

    case WM_WORKER_PROGRESS:
        UI_UpdateProgress(&g_ui, (int)wParam, (int)lParam);
        return 0;

    case WM_WORKER_DONE:
        OnWorkerDone((BOOL)wParam);
        return 0;

    case WM_DESTROY:
        UnregisterHotKey(hwnd, HOTKEY_START);
        if (g_worker) {
            Worker_Stop(g_worker);
            if (g_hThread) {
                WaitForSingleObject(g_hThread, 2000);
                CloseHandle(g_hThread);
            }
            Worker_Free(g_worker);
        }
        Config_Save(&g_cfg, g_iniPath);
        UI_Destroy(&g_ui);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ── 入口点 ───────────────────────────────────────────────

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;

    // 构建 INI 路径：%APPDATA%\KeyboardSim\config.ini
    wchar_t appData[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appData);
    wsprintfW(g_iniPath, L"%s\\KeyboardSim\\config.ini", appData);

    // 确保目录存在
    wchar_t dir[MAX_PATH];
    wsprintfW(dir, L"%s\\KeyboardSim", appData);
    CreateDirectoryW(dir, NULL);

    Config_Load(&g_cfg, g_iniPath);

    // 注册窗口类
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"KeyboardSimClass";
    wc.hIcon         = LoadIconW(NULL, IDI_APPLICATION);
    wc.hIconSm       = LoadIconW(NULL, IDI_APPLICATION);
    RegisterClassExW(&wc);

    // 居中创建窗口
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int winW = 760, winH = 520;
    int x = (screenW - winW) / 2;
    int y = (screenH - winH) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"KeyboardSimClass",
        L"模拟键盘输入工具  v1.0",
        WS_OVERLAPPEDWINDOW,
        x, y, winW, winH,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) return 1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
