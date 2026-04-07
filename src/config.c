#include "config.h"
#include "resource.h"
#include <stdlib.h>
#include <wchar.h>

void Config_Load(AppConfig *cfg, const wchar_t *iniPath)
{
    cfg->delayMs      = (int)GetPrivateProfileIntW(L"Settings", L"DelayMs",
                                                   INTERVAL_DEF, iniPath);
    cfg->darkMode     = (BOOL)GetPrivateProfileIntW(L"Settings", L"DarkMode",
                                                    0, iniPath);
    cfg->alwaysOnTop  = (BOOL)GetPrivateProfileIntW(L"Settings", L"AlwaysOnTop",
                                                    0, iniPath);
    GetPrivateProfileStringW(L"Settings", L"LastDir", L"",
                             cfg->lastDir, MAX_PATH, iniPath);

    // 范围校正
    if (cfg->delayMs < INTERVAL_MIN) cfg->delayMs = INTERVAL_MIN;
    if (cfg->delayMs > INTERVAL_MAX) cfg->delayMs = INTERVAL_MAX;
}

void Config_Save(const AppConfig *cfg, const wchar_t *iniPath)
{
    wchar_t buf[32];

    wsprintfW(buf, L"%d", cfg->delayMs);
    WritePrivateProfileStringW(L"Settings", L"DelayMs", buf, iniPath);

    WritePrivateProfileStringW(L"Settings", L"DarkMode",
                               cfg->darkMode ? L"1" : L"0", iniPath);

    WritePrivateProfileStringW(L"Settings", L"AlwaysOnTop",
                               cfg->alwaysOnTop ? L"1" : L"0", iniPath);

    WritePrivateProfileStringW(L"Settings", L"LastDir",
                               cfg->lastDir, iniPath);
}
