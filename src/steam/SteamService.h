#pragma once

// 模块：Steamworks 生命周期封装
// 未配置 SteamworksSdkDir 时仍会编译为空实现，便于开源贡献者构建。

namespace splite
{

class SteamService
{
public:
    bool Initialize();
    void Update();
    void Shutdown();
    bool IsAvailable() const noexcept;
    bool UnlockAchievement(const char* achievementId);

private:
    bool available_ = false;
};

} // namespace splite
