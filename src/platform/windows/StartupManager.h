#pragma once

// 模块：Windows 开机启动
// 只操作当前用户的 Run 注册表项，不需要管理员权限。

namespace splite
{

class StartupManager
{
public:
    static bool IsEnabled();
    static bool SetEnabled(bool enabled);
};

} // namespace splite
