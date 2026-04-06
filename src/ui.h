#pragma once
#include <windows.h>

typedef struct {
    HWND hwndMain;
    HWND hwndEditText;
    HWND hwndBtnLoad;
    HWND hwndBtnPrepare;
    HWND hwndBtnStart;
    HWND hwndBtnPause;
    HWND hwndBtnStop;
    HWND hwndTrackbar;
    HWND hwndEditInterval;
    HWND hwndProgress;
    HWND hwndStatus;
    HWND hwndComboPreset;
    HWND hwndStaticInterval;
    HWND hwndStaticPreset;
    HWND hwndStaticChars;

    HFONT hFontUI;
    HFONT hFontEdit;
    BOOL  darkMode;
} AppUI;

void UI_Create(HWND hwndParent, AppUI *ui);
void UI_Layout(AppUI *ui, int cx, int cy);
void UI_SetState(AppUI *ui, int state);
void UI_UpdateProgress(AppUI *ui, int current, int total);
void UI_SetStatus(AppUI *ui, const wchar_t *text);
void UI_SetIntervalText(AppUI *ui, int delayMs);
void UI_SetPresetSelection(AppUI *ui, int delayMs);
void UI_ApplyWindowStyling(HWND hwnd, BOOL darkMode);
void UI_Destroy(AppUI *ui);
