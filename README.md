# 模拟键盘输入工具

> 纯 C + Win32 API 桌面小工具，把文本逐字符模拟键盘发送到任意输入框，绕过"禁止粘贴"限制，完整支持中文。

---

## 功能特性

- **逐字符模拟输入** — 使用 `SendInput + KEYEVENTF_UNICODE`，原生支持中文及所有 Unicode 字符
- **从文件加载** — 支持 `.txt` / `.md` 等文本文件，自动识别 UTF-8 / UTF-16 编码
- **可调速度** — 慢速 300ms / 中速 80ms / 快速 20ms 预设，也可手动输入任意间隔（10~500ms）
- **暂停 / 继续 / 停止** — 随时中断，不影响主界面响应
- **实时进度** — 进度条 + 已输入字符数显示
- **全局热键** — `Ctrl+Alt+V` 直接触发开始
- **配置持久化** — 上次速度设置自动保存，下次启动恢复
- **线程隔离** — 输入模拟在独立工作线程，UI 不卡顿

## 截图

![主界面](screenshot1.png)

![数据库功能](screenshot2.png)

## 使用方法

1. 运行 `KeyboardSim.exe`
2. 在左侧文本框粘贴或输入文字，或点击 **从文件加载**
3. 右侧调节字符间隔（默认 80ms，网站有反爬时建议调高到 150ms+）
4. 点击 **准备输入** — 弹出提示后，将鼠标光标点进目标输入框
5. 点击 **开始输入**（或按 `Ctrl+Alt+V`）
6. 需要中断时点 **暂停** 或 **停止**

> **注意**：部分需要管理员权限的程序（如 UAC 弹窗）需以管理员身份运行本工具才能模拟输入。

## 下载

前往 [Releases](../../releases) 页面下载最新版 `KeyboardSim.exe`，无需安装，双击即用。

## 从源码编译

**依赖**：MinGW（gcc 6.3+）

```bat
git clone https://github.com/shenyuhao1/cc-project.git
cd cc-project
build.bat
```

编译产物为 `KeyboardSim.exe`，体积约 66KB，无外部 DLL 依赖。

## 技术栈

| 模块 | 方案 |
|------|------|
| GUI | Win32 API + ComCtl32 v6 |
| 键盘模拟 | `SendInput` + `KEYEVENTF_UNICODE` |
| 中文支持 | 直接发送 `wchar_t`，不依赖输入法 |
| 线程 | `_beginthreadex` + `CRITICAL_SECTION` + Event |
| 配置 | INI 文件（`%APPDATA%\KeyboardSim\config.ini`） |
| 编译 | MinGW gcc，纯 C，无第三方库 |

## License

MIT
