#include "floatbar.h"
#include "resource.h"
#include <windows.h>

#define FLOATBAR_W  180
#define FLOATBAR_H  28
#define BTN_INPUT_W 90
#define BTN_FULL_W  80

static const wchar_t FLOAT_CLASS[] = L"KeyboardSimFloatBar";

static LRESULT CALLBACK FloatBarWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    FloatBar *fb = (FloatBar *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_NCHITTEST:
        return HTCAPTION;  // 允许拖动

    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;  // 不抢焦点

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, fb ? fb->hbrBg : (HBRUSH)(COLOR_WINDOW + 1));
        return 1;
    }

    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wParam;
        if (fb && fb->darkMode) {
            SetTextColor(hdc, RGB(220, 220, 220));
            SetBkColor(hdc, RGB(45, 45, 48));
            return (LRESULT)fb->hbrBg;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    case WM_COMMAND:
        if (fb) {
            int id = LOWORD(wParam);
            if (id == IDC_FLOAT_BTN_INPUT && fb->OnSimInput) {
                fb->OnSimInput();
            } else if (id == IDC_FLOAT_BTN_FULL && fb->OnOpenFull) {
                fb->OnOpenFull();
            }
        }
        return 0;

    case WM_DESTROY:
        if (fb) {
            fb->hwndFloat = NULL;
            fb->hwndBtnInput = NULL;
            fb->hwndBtnFull = NULL;
            fb->visible = FALSE;
        }
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static BOOL s_classRegistered = FALSE;

static void RegisterFloatClass(HINSTANCE hInst)
{
    if (s_classRegistered) return;

    WNDCLASSEXW wc = {0};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = FloatBarWndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = FLOAT_CLASS;

    RegisterClassExW(&wc);
    s_classRegistered = TRUE;
}

BOOL FloatBar_Create(FloatBar *fb, HINSTANCE hInst, BOOL darkMode)
{
    RegisterFloatClass(hInst);

    fb->darkMode = darkMode;
    fb->visible  = FALSE;
    fb->OnSimInput = NULL;
    fb->OnOpenFull = NULL;

    COLORREF bg = darkMode ? RGB(45, 45, 48) : RGB(240, 240, 240);
    fb->hbrBg = CreateSolidBrush(bg);

    fb->hFont = CreateFontW(
        14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");

    fb->hwndFloat = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        FLOAT_CLASS, L"",
        WS_POPUP,
        0, 0, FLOATBAR_W, FLOATBAR_H,
        NULL, NULL, hInst, NULL);

    if (!fb->hwndFloat) return FALSE;

    SetWindowLongPtrW(fb->hwndFloat, GWLP_USERDATA, (LONG_PTR)fb);

    SetLayeredWindowAttributes(fb->hwndFloat, 0, 230, LWA_ALPHA);

    fb->hwndBtnInput = CreateWindowExW(
        0, L"BUTTON", L"模拟输入",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        2, 2, BTN_INPUT_W, FLOATBAR_H - 4,
        fb->hwndFloat, (HMENU)IDC_FLOAT_BTN_INPUT, hInst, NULL);

    fb->hwndBtnFull = CreateWindowExW(
        0, L"BUTTON", L"完整版",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        BTN_INPUT_W + 4, 2, BTN_FULL_W, FLOATBAR_H - 4,
        fb->hwndFloat, (HMENU)IDC_FLOAT_BTN_FULL, hInst, NULL);

    SendMessageW(fb->hwndBtnInput, WM_SETFONT, (WPARAM)fb->hFont, TRUE);
    SendMessageW(fb->hwndBtnFull, WM_SETFONT, (WPARAM)fb->hFont, TRUE);

    return TRUE;
}

void FloatBar_ShowAt(FloatBar *fb, int x, int y)
{
    if (!fb || !fb->hwndFloat) return;

    // 确保不超出屏幕
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    if (x + FLOATBAR_W > screenW) x = screenW - FLOATBAR_W;
    if (y + FLOATBAR_H > screenH) y = screenH - FLOATBAR_H;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    SetWindowPos(fb->hwndFloat, HWND_TOPMOST,
                 x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    fb->visible = TRUE;
}

void FloatBar_Hide(FloatBar *fb)
{
    if (!fb || !fb->hwndFloat) return;
    ShowWindow(fb->hwndFloat, SW_HIDE);
    fb->visible = FALSE;
}

void FloatBar_SetDarkMode(FloatBar *fb, BOOL darkMode)
{
    if (!fb) return;
    fb->darkMode = darkMode;
    if (fb->hbrBg) DeleteObject(fb->hbrBg);
    COLORREF bg = darkMode ? RGB(45, 45, 48) : RGB(240, 240, 240);
    fb->hbrBg = CreateSolidBrush(bg);
    InvalidateRect(fb->hwndFloat, NULL, TRUE);
}

void FloatBar_Destroy(FloatBar *fb)
{
    if (!fb) return;
    if (fb->hwndFloat) {
        DestroyWindow(fb->hwndFloat);
        fb->hwndFloat = NULL;
    }
    if (fb->hFont) {
        DeleteObject(fb->hFont);
        fb->hFont = NULL;
    }
    if (fb->hbrBg) {
        DeleteObject(fb->hbrBg);
        fb->hbrBg = NULL;
    }
    fb->visible = FALSE;
}
