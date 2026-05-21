#pragma once
#include <windows.h>

typedef struct {
    HWND    hwndFloat;          // 浮动条窗口
    HWND    hwndBtnInput;       // "模拟输入"按钮
    HWND    hwndBtnFull;        // "完整版"按钮
    HFONT   hFont;              // 字体
    HBRUSH  hbrBg;              // 背景画刷
    BOOL    darkMode;           // 暗色模式
    BOOL    visible;            // 当前是否可见

    // 回调
    void    (*OnSimInput)(void);   // 点击"模拟输入"
    void    (*OnOpenFull)(void);   // 点击"完整版"
} FloatBar;

// 创建浮动条（不显示）
BOOL FloatBar_Create(FloatBar *fb, HINSTANCE hInst, BOOL darkMode);

// 在指定屏幕坐标处显示浮动条
void FloatBar_ShowAt(FloatBar *fb, int x, int y);

// 隐藏浮动条
void FloatBar_Hide(FloatBar *fb);

// 更新主题
void FloatBar_SetDarkMode(FloatBar *fb, BOOL darkMode);

// 销毁浮动条
void FloatBar_Destroy(FloatBar *fb);
