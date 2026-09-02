#pragma once

// 模块：本地配置
// 职责：把无需账号同步的轻量设置保存到 LocalAppData。

#include <string>

namespace splite
{

struct AppConfig
{
    bool topMost = true;

    static AppConfig Load();
    bool Save() const;
    static std::wstring GetConfigPath();
};

} // namespace splite
