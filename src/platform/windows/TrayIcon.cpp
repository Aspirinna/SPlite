#include "TrayIcon.h"

#include "../../Resource.h"

namespace splite
{

bool TrayIcon::Initialize(HWND window, HINSTANCE instance, UINT callbackMessage)
{
    data_.cbSize = sizeof(data_);
    data_.hWnd = window;
    data_.uID = 1;
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data_.uCallbackMessage = callbackMessage;
    data_.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_SPLITE));
    wcscpy_s(data_.szTip, L"SPlite 桌面宠物");

    added_ = Shell_NotifyIconW(NIM_ADD, &data_) != FALSE;
    if (added_)
    {
        data_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data_);
    }
    return added_;
}

void TrayIcon::Shutdown()
{
    if (added_)
    {
        Shell_NotifyIconW(NIM_DELETE, &data_);
        added_ = false;
    }
}

TrayIcon::~TrayIcon()
{
    Shutdown();
}

} // namespace splite
