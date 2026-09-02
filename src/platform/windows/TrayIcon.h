#pragma once

// 模块：Windows 系统托盘图标
// 职责：为无任务栏窗口提供稳定的设置和退出入口。

#include <windows.h>
#include <shellapi.h>

namespace splite
{

class TrayIcon
{
public:
    bool Initialize(HWND window, HINSTANCE instance, UINT callbackMessage);
    void Shutdown();
    ~TrayIcon();

private:
    NOTIFYICONDATAW data_{};
    bool added_ = false;
};

} // namespace splite
