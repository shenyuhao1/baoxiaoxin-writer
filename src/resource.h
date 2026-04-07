#pragma once

// ── 控件 ID ──────────────────────────────────────────────
#define IDC_EDIT_TEXT       1001
#define IDC_BTN_LOAD        1002
#define IDC_BTN_PREPARE     1003
#define IDC_BTN_START       1004
#define IDC_BTN_PAUSE       1005
#define IDC_BTN_STOP        1006
#define IDC_TRACKBAR_SPEED  1007
#define IDC_EDIT_INTERVAL   1008
#define IDC_PROGRESS        1009
#define IDC_STATUS          1010
#define IDC_COMBO_PRESET    1011
#define IDC_STATIC_INTERVAL 1012
#define IDC_STATIC_PRESET   1013
#define IDC_STATIC_CHARS    1014
#define IDC_CHK_TOPMOST     1015

// ── 速度预设（毫秒/字符）────────────────────────────────
#define SPEED_SLOW    300
#define SPEED_MEDIUM   80
#define SPEED_FAST     20

// ── Trackbar 范围 ────────────────────────────────────────
#define INTERVAL_MIN   10
#define INTERVAL_MAX  500
#define INTERVAL_DEF   80

// ── 全局热键 ID ──────────────────────────────────────────
#define HOTKEY_START    0xBEEF

// ── 自定义窗口消息 ───────────────────────────────────────
// wParam = 已输入字符数, lParam = 总字符数
#define WM_WORKER_PROGRESS  (WM_USER + 1)
// wParam = 0 正常完成, 1 = 被停止
#define WM_WORKER_DONE      (WM_USER + 2)

// ── 应用状态机 ───────────────────────────────────────────
#define STATE_IDLE      0   // 空闲
#define STATE_READY     1   // 已准备（等待用户切换焦点）
#define STATE_RUNNING   2   // 正在输入
#define STATE_PAUSED    3   // 已暂停
