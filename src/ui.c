#define _WIN32_WINNT 0x0601
#define _WIN32_IE    0x0600
#include "ui.h"
#include "resource.h"
#include <commctrl.h>
#include <uxtheme.h>
#include <stdint.h>
#include <stdio.h>

// ── 内部工具 ─────────────────────────────────────────────

typedef HRESULT (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);

static void ApplyDarkMode(HWND hwnd, BOOL dark)
{
    HMODULE hDwm = LoadLibraryW(L"dwmapi.dll");
    if (!hDwm) return;
    PFN_DwmSetWindowAttribute pfn =
        (PFN_DwmSetWindowAttribute)GetProcAddress(hDwm, "DwmSetWindowAttribute");
    if (pfn) {
        BOOL val = dark ? TRUE : FALSE;
        pfn(hwnd, 20, &val, sizeof(val)); // DWMWA_USE_IMMERSIVE_DARK_MODE
    }
    FreeLibrary(hDwm);
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

// 在 AppUI 中按 HWND 找按钮槽位，返回 -1 表示未找到
static int FindBtnIdx(AppUI *ui, HWND hwnd)
{
    for (int i = 0; i < ui->btnCount; i++) {
        if (ui->btnHwnds[i] == hwnd) return i;
    }
    return -1;
}

// ── 按钮子类化（捕捉 WM_MOUSELEAVE） ────────────────────

static WNDPROC s_origBtnProc = NULL;

static LRESULT CALLBACK BtnSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    AppUI *ui = (AppUI *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_MOUSEMOVE:
        if (ui) UI_OnMouseMove(ui, hwnd);
        break;
    case WM_MOUSELEAVE:
        if (ui) UI_OnMouseLeave(ui, hwnd);
        break;
    case WM_LBUTTONDOWN:
        if (ui) {
            int i = FindBtnIdx(ui, hwnd);
            if (i >= 0) ui->btnPressed[i] = TRUE;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;
    case WM_LBUTTONUP:
        if (ui) {
            int i = FindBtnIdx(ui, hwnd);
            if (i >= 0) ui->btnPressed[i] = FALSE;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;
    }
    return CallWindowProcW(s_origBtnProc, hwnd, msg, wParam, lParam);
}

static HWND MakeOwnerDrawBtn(HWND parent, const wchar_t *text, int id,
                              HFONT font, AppUI *ui, BtnRole role)
{
    HWND hwnd = CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 0, 0, parent, (HMENU)(intptr_t)id, NULL, NULL);
    SendMessageW(hwnd, WM_SETFONT, (WPARAM)font, TRUE);

    // 存入 AppUI
    if (ui->btnCount < MAX_BTNS) {
        int i = ui->btnCount++;
        ui->btnHwnds[i]   = hwnd;
        ui->btnRoles[i]   = role;
        ui->btnHover[i]   = FALSE;
        ui->btnPressed[i] = FALSE;
        ui->btnTracking[i]= FALSE;
    }

    // 子类化
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)ui);
    if (!s_origBtnProc)
        s_origBtnProc = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)BtnSubclassProc);
    else
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)BtnSubclassProc);

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

// ── 公开接口 ─────────────────────────────────────────────

void UI_Create(HWND hwndParent, AppUI *ui)
{
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_WIN95_CLASSES | ICC_PROGRESS_CLASS | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    ui->hwndMain  = hwndParent;
    ui->darkMode  = TRUE;  // 默认暗色
    ui->btnCount  = 0;
    ui->hFontUI   = CreateUIFont(10, FALSE);
    ui->hFontEdit = CreateUIFont(11, FALSE);

    // 缓存画刷
    ui->hbrBg    = CreateSolidBrush(CLR_BG_DARK);
    ui->hbrPanel = CreateSolidBrush(CLR_PANEL_DARK);
    ui->hbrEdit  = CreateSolidBrush(CLR_EDIT_DARK);

    // 多行文本框
    ui->hwndEditText = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
        0, 0, 0, 0, hwndParent,
        (HMENU)IDC_EDIT_TEXT, NULL, NULL);
    SendMessageW(ui->hwndEditText, WM_SETFONT, (WPARAM)ui->hFontEdit, TRUE);
    SendMessageW(ui->hwndEditText, EM_SETLIMITTEXT, 0, 0);
    // 移除 WS_EX_CLIENTEDGE 改用纯色背景，稍微扁平
    SetWindowTheme(ui->hwndEditText, L" ", L" ");

    // 右侧按钮（owner-draw）
    ui->hwndBtnLoad    = MakeOwnerDrawBtn(hwndParent, L"从文件加载", IDC_BTN_LOAD,    ui->hFontUI, ui, BTN_ROLE_NEUTRAL);
    ui->hwndBtnPrepare = MakeOwnerDrawBtn(hwndParent, L"准备输入",   IDC_BTN_PREPARE, ui->hFontUI, ui, BTN_ROLE_NEUTRAL);
    ui->hwndBtnStart   = MakeOwnerDrawBtn(hwndParent, L"开始输入",   IDC_BTN_START,   ui->hFontUI, ui, BTN_ROLE_ACCENT);
    ui->hwndBtnPause   = MakeOwnerDrawBtn(hwndParent, L"暂停",       IDC_BTN_PAUSE,   ui->hFontUI, ui, BTN_ROLE_WARN);
    ui->hwndBtnStop    = MakeOwnerDrawBtn(hwndParent, L"停止",       IDC_BTN_STOP,    ui->hFontUI, ui, BTN_ROLE_DANGER);

    // 速度预设
    ui->hwndStaticPreset = MakeStatic(hwndParent, L"速度预设：", IDC_STATIC_PRESET, ui->hFontUI);
    ui->hwndComboPreset  = CreateWindowExW(0, L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 0, 0, hwndParent, (HMENU)IDC_COMBO_PRESET, NULL, NULL);
    SendMessageW(ui->hwndComboPreset, WM_SETFONT, (WPARAM)ui->hFontUI, TRUE);
    SendMessageW(ui->hwndComboPreset, CB_ADDSTRING, 0, (LPARAM)L"慢速 (300ms)");
    SendMessageW(ui->hwndComboPreset, CB_ADDSTRING, 0, (LPARAM)L"中速 (80ms)");
    SendMessageW(ui->hwndComboPreset, CB_ADDSTRING, 0, (LPARAM)L"快速 (20ms)");
    SendMessageW(ui->hwndComboPreset, CB_SETCURSEL, 1, 0);

    // 间隔调节
    ui->hwndStaticInterval = MakeStatic(hwndParent, L"字符间隔 (ms)：", IDC_STATIC_INTERVAL, ui->hFontUI);

    ui->hwndTrackbar = CreateWindowExW(0, TRACKBAR_CLASSW, NULL,
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS | TBS_BOTH,
        0, 0, 0, 0, hwndParent, (HMENU)IDC_TRACKBAR_SPEED, NULL, NULL);
    SetWindowTheme(ui->hwndTrackbar, L"Explorer", NULL); // 细滑道风格
    SendMessageW(ui->hwndTrackbar, TBM_SETRANGE, TRUE, MAKELPARAM(INTERVAL_MIN, INTERVAL_MAX));
    SendMessageW(ui->hwndTrackbar, TBM_SETPOS, TRUE, INTERVAL_DEF);

    ui->hwndEditInterval = CreateWindowExW(0, L"EDIT", L"80",
        WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_CENTER,
        0, 0, 0, 0, hwndParent, (HMENU)IDC_EDIT_INTERVAL, NULL, NULL);
    SendMessageW(ui->hwndEditInterval, WM_SETFONT, (WPARAM)ui->hFontUI, TRUE);
    SetWindowTheme(ui->hwndEditInterval, L" ", L" ");

    // 进度条 + 颜色
    ui->hwndProgress = CreateWindowExW(0, PROGRESS_CLASSW, NULL,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        0, 0, 0, 0, hwndParent, (HMENU)IDC_PROGRESS, NULL, NULL);
    SendMessageW(ui->hwndProgress, PBM_SETBARCOLOR, 0, CLR_ACCENT);
    SendMessageW(ui->hwndProgress, PBM_SETBKCOLOR, 0, CLR_PANEL_DARK);

    // 字符计数 + 状态栏
    ui->hwndStaticChars = MakeStatic(hwndParent, L"0 / 0 字符", IDC_STATIC_CHARS, ui->hFontUI);
    ui->hwndStatus      = MakeStatic(hwndParent, L"就绪",        IDC_STATUS,       ui->hFontUI);
    ui->hwndChkTopmost  = CreateWindowExW(0, L"BUTTON", L"窗口置顶",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        0, 0, 0, 0, hwndParent, (HMENU)IDC_CHK_TOPMOST, NULL, NULL);
    SendMessageW(ui->hwndChkTopmost, WM_SETFONT, (WPARAM)ui->hFontUI, TRUE);

    UI_SetState(ui, STATE_IDLE);
}

void UI_Layout(AppUI *ui, int cx, int cy)
{
    int pad     = 12;
    int btnH    = 34;
    int editW   = cx * 60 / 100;
    int panelX  = editW + pad * 2;
    int panelW  = cx - panelX - pad;
    int statusH = 24;
    int progressH = 6;
    int bottomH = statusH + progressH + pad * 2 + 4;

    // 文本框
    SetWindowPos(ui->hwndEditText, NULL,
        pad, pad,
        editW - pad, cy - bottomH - pad * 2,
        SWP_NOZORDER);

    int y = pad;

    SetWindowPos(ui->hwndBtnLoad, NULL, panelX, y, panelW, btnH, SWP_NOZORDER);
    y += btnH + pad;

    SetWindowPos(ui->hwndStaticPreset, NULL, panelX, y, panelW, 18, SWP_NOZORDER);
    y += 22;
    SetWindowPos(ui->hwndComboPreset, NULL, panelX, y, panelW, 120, SWP_NOZORDER);
    y += btnH + pad;

    SetWindowPos(ui->hwndStaticInterval, NULL, panelX, y, panelW, 18, SWP_NOZORDER);
    y += 22;
    SetWindowPos(ui->hwndTrackbar, NULL, panelX, y, panelW, 26, SWP_NOZORDER);
    y += 28;
    SetWindowPos(ui->hwndEditInterval, NULL,
        panelX + panelW/2 - 28, y, 56, 24, SWP_NOZORDER);
    y += 30 + pad;

    SetWindowPos(ui->hwndBtnPrepare, NULL, panelX, y, panelW, btnH, SWP_NOZORDER);
    y += btnH + 6;
    SetWindowPos(ui->hwndBtnStart, NULL, panelX, y, panelW, btnH, SWP_NOZORDER);
    y += btnH + pad;

    int halfW = (panelW - 6) / 2;
    SetWindowPos(ui->hwndBtnPause, NULL, panelX, y, halfW, btnH, SWP_NOZORDER);
    SetWindowPos(ui->hwndBtnStop,  NULL, panelX + halfW + 6, y, halfW, btnH, SWP_NOZORDER);
    y += btnH + pad;

    SetWindowPos(ui->hwndStaticChars, NULL, panelX, y, panelW, 18, SWP_NOZORDER);

    // 底部
    int bottomY = cy - bottomH;
    SetWindowPos(ui->hwndProgress, NULL,
        0, bottomY,
        cx, progressH, SWP_NOZORDER);
    int chkW = 90;
    SetWindowPos(ui->hwndStatus, NULL,
        pad, bottomY + progressH + pad,
        cx - pad * 3 - chkW, statusH, SWP_NOZORDER);
    SetWindowPos(ui->hwndChkTopmost, NULL,
        cx - pad - chkW, bottomY + progressH + pad,
        chkW, statusH, SWP_NOZORDER);
}

void UI_SetState(AppUI *ui, int state)
{
    EnableWindow(ui->hwndBtnLoad,      state == STATE_IDLE || state == STATE_READY);
    EnableWindow(ui->hwndBtnPrepare,   state == STATE_IDLE);
    EnableWindow(ui->hwndBtnStart,     state == STATE_READY);
    EnableWindow(ui->hwndBtnPause,     state == STATE_RUNNING || state == STATE_PAUSED);
    EnableWindow(ui->hwndBtnStop,      state == STATE_RUNNING || state == STATE_PAUSED);
    EnableWindow(ui->hwndTrackbar,     state == STATE_IDLE || state == STATE_READY);
    EnableWindow(ui->hwndEditInterval, state == STATE_IDLE || state == STATE_READY);
    EnableWindow(ui->hwndComboPreset,  state == STATE_IDLE || state == STATE_READY);

    SetWindowTextW(ui->hwndBtnPause, state == STATE_PAUSED ? L"继续" : L"暂停");

    // 强制重绘按钮（状态切换后颜色更新）
    for (int i = 0; i < ui->btnCount; i++)
        InvalidateRect(ui->btnHwnds[i], NULL, FALSE);
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
    if      (delayMs >= SPEED_SLOW)   SendMessageW(ui->hwndComboPreset, CB_SETCURSEL, 0, 0);
    else if (delayMs >= SPEED_MEDIUM) SendMessageW(ui->hwndComboPreset, CB_SETCURSEL, 1, 0);
    else                              SendMessageW(ui->hwndComboPreset, CB_SETCURSEL, 2, 0);
}

void UI_ApplyWindowStyling(HWND hwnd, BOOL darkMode)
{
    ApplyDarkMode(hwnd, darkMode);
}

void UI_Destroy(AppUI *ui)
{
    if (ui->hFontUI)   { DeleteObject(ui->hFontUI);   ui->hFontUI   = NULL; }
    if (ui->hFontEdit) { DeleteObject(ui->hFontEdit); ui->hFontEdit = NULL; }
    if (ui->hbrBg)     { DeleteObject(ui->hbrBg);     ui->hbrBg     = NULL; }
    if (ui->hbrPanel)  { DeleteObject(ui->hbrPanel);  ui->hbrPanel  = NULL; }
    if (ui->hbrEdit)   { DeleteObject(ui->hbrEdit);   ui->hbrEdit   = NULL; }
}

// ── owner-draw 绘制按钮 ──────────────────────────────────

LRESULT UI_OnDrawItem(AppUI *ui, WPARAM wParam, LPARAM lParam)
{
    (void)wParam;
    DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
    if (dis->CtlType != ODT_BUTTON) return FALSE;

    int i = FindBtnIdx(ui, dis->hwndItem);
    if (i < 0) return FALSE;

    BOOL enabled = IsWindowEnabled(dis->hwndItem);
    BOOL hover   = ui->btnHover[i]   && enabled;
    BOOL pressed = ui->btnPressed[i] && enabled;

    // 确定底色
    COLORREF clrFill;
    COLORREF clrText = enabled ? CLR_TEXT_DARK : CLR_TEXT_DIM;

    switch (ui->btnRoles[i]) {
    case BTN_ROLE_ACCENT:
        clrFill = pressed ? CLR_ACCENT_PRESS
                : hover   ? CLR_ACCENT_HOVER
                :           CLR_ACCENT;
        break;
    case BTN_ROLE_WARN:
        clrFill = pressed ? RGB(160, 100, 10)
                : hover   ? RGB(220, 160, 50)
                :           CLR_BTN_WARN;
        break;
    case BTN_ROLE_DANGER:
        clrFill = pressed ? RGB(170, 30, 30)
                : hover   ? RGB(240, 80, 80)
                :           CLR_BTN_DANGER;
        break;
    default: // NEUTRAL
        clrFill = pressed ? RGB(35, 36, 50)
                : hover   ? CLR_BTN_NEUTRAL_H
                :           CLR_BTN_NEUTRAL;
        break;
    }

    if (!enabled) clrFill = RGB(40, 41, 55);

    HDC  hdc  = dis->hDC;
    RECT rc   = dis->rcItem;
    int  r    = 6;  // 圆角半径

    // 填充圆角矩形
    HBRUSH hbr = CreateSolidBrush(clrFill);
    HRGN   rgn = CreateRoundRectRgn(rc.left, rc.top, rc.right, rc.bottom, r*2, r*2);
    FillRgn(hdc, rgn, hbr);
    DeleteObject(rgn);
    DeleteObject(hbr);

    // 文字
    wchar_t text[64] = {0};
    GetWindowTextW(dis->hwndItem, text, 63);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, clrText);
    HFONT oldFont = (HFONT)SelectObject(hdc, ui->hFontUI);
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);

    return TRUE;
}

// ── 控件背景着色 ──────────────────────────────────────────

LRESULT UI_OnCtlColor(AppUI *ui, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)hwnd;
    (void)msg;
    HDC   hdc     = (HDC)wParam;
    HWND  hwndCtl = (HWND)lParam;

    if (hwndCtl == ui->hwndEditText || hwndCtl == ui->hwndEditInterval) {
        SetTextColor(hdc, CLR_TEXT_DARK);
        SetBkColor(hdc, CLR_EDIT_DARK);
        return (LRESULT)ui->hbrEdit;
    }

    // 所有 STATIC 标签
    SetTextColor(hdc, CLR_TEXT_DIM);
    SetBkColor(hdc, CLR_BG_DARK);
    return (LRESULT)ui->hbrBg;
}

// ── 鼠标 hover 跟踪 ──────────────────────────────────────

void UI_OnMouseMove(AppUI *ui, HWND hwndBtn)
{
    int i = FindBtnIdx(ui, hwndBtn);
    if (i < 0) return;

    if (!ui->btnTracking[i]) {
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwndBtn, 0 };
        TrackMouseEvent(&tme);
        ui->btnTracking[i] = TRUE;
    }

    if (!ui->btnHover[i]) {
        ui->btnHover[i] = TRUE;
        InvalidateRect(hwndBtn, NULL, FALSE);
    }
}

void UI_OnMouseLeave(AppUI *ui, HWND hwndBtn)
{
    int i = FindBtnIdx(ui, hwndBtn);
    if (i < 0) return;
    ui->btnHover[i]    = FALSE;
    ui->btnTracking[i] = FALSE;
    InvalidateRect(hwndBtn, NULL, FALSE);
}
