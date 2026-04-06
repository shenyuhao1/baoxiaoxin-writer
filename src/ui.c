#include "ui.h"
#include "resource.h"
#define _WIN32_WINNT 0x0601
#define _WIN32_IE 0x0600
#include <commctrl.h>
#include <uxtheme.h>
#include <stdint.h>
#include <stdio.h>

// 颜色方案
#define CLR_BG_LIGHT      RGB(245, 246, 250)
#define CLR_BG_DARK       RGB(30,  31,  40)
#define CLR_PANEL_LIGHT   RGB(255, 255, 255)
#define CLR_PANEL_DARK    RGB(40,  41,  54)
#define CLR_ACCENT        RGB(99,  102, 241)
#define CLR_TEXT_LIGHT    RGB(30,  30,  30)
#define CLR_TEXT_DARK     RGB(220, 220, 230)

// 动态加载 DwmSetWindowAttribute
typedef HRESULT (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);

static void ApplyDarkMode(HWND hwnd, BOOL dark)
{
    HMODULE hDwm = LoadLibraryW(L"dwmapi.dll");
    if (hDwm) {
        PFN_DwmSetWindowAttribute pfn =
            (PFN_DwmSetWindowAttribute)GetProcAddress(hDwm, "DwmSetWindowAttribute");
        if (pfn) {
            BOOL val = dark ? TRUE : FALSE;
            // DWMWA_USE_IMMERSIVE_DARK_MODE = 20 (Win10 1809+)
            pfn(hwnd, 20, &val, sizeof(val));
        }
        FreeLibrary(hDwm);
    }
}

static HFONT CreateUIFont(int ptSize, BOOL bold)
{
    return CreateFontW(
        -MulDiv(ptSize, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72),
        0, 0, 0,
        bold ? FW_SEMIBOLD : FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS,
        L"Microsoft YaHei UI"
    );
}

static HWND MakeButton(HWND parent, const wchar_t *text, int id, HFONT font)
{
    HWND hwnd = CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
        0, 0, 0, 0, parent, (HMENU)(intptr_t)id, NULL, NULL);
    SendMessageW(hwnd, WM_SETFONT, (WPARAM)font, TRUE);
    return hwnd;
}

static HWND MakeStatic(HWND parent, const wchar_t *text, int id, HFONT font)
{
    HWND hwnd = CreateWindowExW(0, L"STATIC", text,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 0, 0, parent, (HMENU)(intptr_t)id, NULL, NULL);
    SendMessageW(hwnd, WM_SETFONT, (WPARAM)font, TRUE);
    return hwnd;
}

void UI_Create(HWND hwndParent, AppUI *ui)
{
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_WIN95_CLASSES | ICC_PROGRESS_CLASS | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    ui->hwndMain = hwndParent;
    ui->hFontUI  = CreateUIFont(10, FALSE);
    ui->hFontEdit = CreateUIFont(11, FALSE);

    // 多行文本编辑框
    ui->hwndEditText = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
        0, 0, 0, 0, hwndParent,
        (HMENU)IDC_EDIT_TEXT, NULL, NULL);
    SendMessageW(ui->hwndEditText, WM_SETFONT, (WPARAM)ui->hFontEdit, TRUE);
    SendMessageW(ui->hwndEditText, EM_SETLIMITTEXT, 0, 0); // 无限制

    // 右侧控件
    ui->hwndBtnLoad    = MakeButton(hwndParent, L"从文件加载", IDC_BTN_LOAD,    ui->hFontUI);
    ui->hwndBtnPrepare = MakeButton(hwndParent, L"准备输入",   IDC_BTN_PREPARE, ui->hFontUI);
    ui->hwndBtnStart   = MakeButton(hwndParent, L"开始输入",   IDC_BTN_START,   ui->hFontUI);
    ui->hwndBtnPause   = MakeButton(hwndParent, L"暂停",       IDC_BTN_PAUSE,   ui->hFontUI);
    ui->hwndBtnStop    = MakeButton(hwndParent, L"停止",       IDC_BTN_STOP,    ui->hFontUI);

    // 速度预设下拉框
    ui->hwndStaticPreset = MakeStatic(hwndParent, L"速度预设：", IDC_STATIC_PRESET, ui->hFontUI);
    ui->hwndComboPreset  = CreateWindowExW(0, L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 0, 0, hwndParent, (HMENU)IDC_COMBO_PRESET, NULL, NULL);
    SendMessageW(ui->hwndComboPreset, WM_SETFONT, (WPARAM)ui->hFontUI, TRUE);
    SendMessageW(ui->hwndComboPreset, CB_ADDSTRING, 0, (LPARAM)L"慢速 (300ms)");
    SendMessageW(ui->hwndComboPreset, CB_ADDSTRING, 0, (LPARAM)L"中速 (80ms)");
    SendMessageW(ui->hwndComboPreset, CB_ADDSTRING, 0, (LPARAM)L"快速 (20ms)");
    SendMessageW(ui->hwndComboPreset, CB_SETCURSEL, 1, 0); // 默认中速

    // 间隔调节
    ui->hwndStaticInterval = MakeStatic(hwndParent, L"字符间隔 (ms)：", IDC_STATIC_INTERVAL, ui->hFontUI);

    ui->hwndTrackbar = CreateWindowExW(0, TRACKBAR_CLASSW, NULL,
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
        0, 0, 0, 0, hwndParent, (HMENU)IDC_TRACKBAR_SPEED, NULL, NULL);
    SendMessageW(ui->hwndTrackbar, TBM_SETRANGE, TRUE,
                 MAKELPARAM(INTERVAL_MIN, INTERVAL_MAX));
    SendMessageW(ui->hwndTrackbar, TBM_SETPOS, TRUE, INTERVAL_DEF);
    SendMessageW(ui->hwndTrackbar, TBM_SETTICFREQ, 50, 0);

    ui->hwndEditInterval = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"80",
        WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_CENTER,
        0, 0, 0, 0, hwndParent, (HMENU)IDC_EDIT_INTERVAL, NULL, NULL);
    SendMessageW(ui->hwndEditInterval, WM_SETFONT, (WPARAM)ui->hFontUI, TRUE);

    // 进度条
    ui->hwndProgress = CreateWindowExW(0, PROGRESS_CLASSW, NULL,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        0, 0, 0, 0, hwndParent, (HMENU)IDC_PROGRESS, NULL, NULL);

    // 字符计数
    ui->hwndStaticChars = MakeStatic(hwndParent, L"0 / 0 字符", IDC_STATIC_CHARS, ui->hFontUI);

    // 状态栏
    ui->hwndStatus = MakeStatic(hwndParent, L"就绪", IDC_STATUS, ui->hFontUI);

    // 初始按钮状态
    UI_SetState(ui, STATE_IDLE);
}

void UI_Layout(AppUI *ui, int cx, int cy)
{
    int pad    = 10;
    int btnH   = 32;
    int editW  = cx * 60 / 100;
    int panelX = editW + pad * 2;
    int panelW = cx - panelX - pad;
    int statusH = 24;
    int progressH = 18;
    int bottomH = statusH + progressH + pad * 2;

    // 文本编辑框（左侧）
    SetWindowPos(ui->hwndEditText, NULL,
        pad, pad,
        editW - pad, cy - bottomH - pad * 2,
        SWP_NOZORDER);

    // 右侧面板 y 坐标
    int y = pad;

    // 加载文件按钮
    SetWindowPos(ui->hwndBtnLoad, NULL, panelX, y, panelW, btnH, SWP_NOZORDER);
    y += btnH + pad;

    // 速度预设
    SetWindowPos(ui->hwndStaticPreset, NULL, panelX, y, panelW, 18, SWP_NOZORDER);
    y += 20;
    SetWindowPos(ui->hwndComboPreset, NULL, panelX, y, panelW, 120, SWP_NOZORDER);
    y += btnH + pad;

    // 间隔标签
    SetWindowPos(ui->hwndStaticInterval, NULL, panelX, y, panelW, 18, SWP_NOZORDER);
    y += 22;

    // Trackbar
    SetWindowPos(ui->hwndTrackbar, NULL, panelX, y, panelW, 28, SWP_NOZORDER);
    y += 30;

    // 间隔数值编辑框
    SetWindowPos(ui->hwndEditInterval, NULL,
        panelX + panelW / 2 - 30, y, 60, 24, SWP_NOZORDER);
    y += 30 + pad;

    // 准备 / 开始
    SetWindowPos(ui->hwndBtnPrepare, NULL, panelX, y, panelW, btnH, SWP_NOZORDER);
    y += btnH + pad / 2;
    SetWindowPos(ui->hwndBtnStart, NULL, panelX, y, panelW, btnH, SWP_NOZORDER);
    y += btnH + pad;

    // 暂停 / 停止（并排）
    int halfW = (panelW - pad / 2) / 2;
    SetWindowPos(ui->hwndBtnPause, NULL, panelX, y, halfW, btnH, SWP_NOZORDER);
    SetWindowPos(ui->hwndBtnStop,  NULL, panelX + halfW + pad / 2, y, halfW, btnH, SWP_NOZORDER);
    y += btnH + pad;

    // 字符计数
    SetWindowPos(ui->hwndStaticChars, NULL, panelX, y, panelW, 18, SWP_NOZORDER);

    // 底部：进度条 + 状态
    int bottomY = cy - bottomH;
    SetWindowPos(ui->hwndProgress, NULL,
        pad, bottomY,
        cx - pad * 2, progressH, SWP_NOZORDER);
    SetWindowPos(ui->hwndStatus, NULL,
        pad, bottomY + progressH + pad,
        cx - pad * 2, statusH, SWP_NOZORDER);
}

void UI_SetState(AppUI *ui, int state)
{
    EnableWindow(ui->hwndBtnLoad,    state == STATE_IDLE || state == STATE_READY);
    EnableWindow(ui->hwndBtnPrepare, state == STATE_IDLE);
    EnableWindow(ui->hwndBtnStart,   state == STATE_READY);
    EnableWindow(ui->hwndBtnPause,   state == STATE_RUNNING || state == STATE_PAUSED);
    EnableWindow(ui->hwndBtnStop,    state == STATE_RUNNING || state == STATE_PAUSED);
    EnableWindow(ui->hwndTrackbar,   state == STATE_IDLE || state == STATE_READY);
    EnableWindow(ui->hwndEditInterval, state == STATE_IDLE || state == STATE_READY);
    EnableWindow(ui->hwndComboPreset,  state == STATE_IDLE || state == STATE_READY);

    if (state == STATE_PAUSED) {
        SetWindowTextW(ui->hwndBtnPause, L"继续");
    } else {
        SetWindowTextW(ui->hwndBtnPause, L"暂停");
    }
}

void UI_UpdateProgress(AppUI *ui, int current, int total)
{
    wchar_t buf[64];
    if (total > 0) {
        SendMessageW(ui->hwndProgress, PBM_SETRANGE32, 0, total);
        SendMessageW(ui->hwndProgress, PBM_SETPOS, current, 0);
    }
    wsprintfW(buf, L"%d / %d 字符", current, total);
    SetWindowTextW(ui->hwndStaticChars, buf);
}

void UI_SetStatus(AppUI *ui, const wchar_t *text)
{
    SetWindowTextW(ui->hwndStatus, text);
}

void UI_SetIntervalText(AppUI *ui, int delayMs)
{
    wchar_t buf[16];
    wsprintfW(buf, L"%d", delayMs);
    SetWindowTextW(ui->hwndEditInterval, buf);
}

void UI_SetPresetSelection(AppUI *ui, int delayMs)
{
    if (delayMs >= SPEED_SLOW)        SendMessageW(ui->hwndComboPreset, CB_SETCURSEL, 0, 0);
    else if (delayMs >= SPEED_MEDIUM) SendMessageW(ui->hwndComboPreset, CB_SETCURSEL, 1, 0);
    else                              SendMessageW(ui->hwndComboPreset, CB_SETCURSEL, 2, 0);
}

void UI_ApplyWindowStyling(HWND hwnd, BOOL darkMode)
{
    ApplyDarkMode(hwnd, darkMode);

    // 圆角（Win11 原生支持，Win10 用 Region 模拟）
    RECT rc;
    GetClientRect(hwnd, &rc);
    HRGN hRgn = CreateRoundRectRgn(0, 0, rc.right, rc.bottom, 12, 12);
    SetWindowRgn(hwnd, hRgn, TRUE);
}

void UI_Destroy(AppUI *ui)
{
    if (ui->hFontUI)   { DeleteObject(ui->hFontUI);   ui->hFontUI   = NULL; }
    if (ui->hFontEdit) { DeleteObject(ui->hFontEdit); ui->hFontEdit = NULL; }
}
