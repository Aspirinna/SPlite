#pragma once

// Spine Runtime 的稳定接入边界。
// 公开仓库默认不包含受 Spine 许可约束的源码；取得许可证并放入依赖目录后，
// 可启用 SPLITE_ENABLE_SPINE，由实现文件连接 spine-cpp 4.3。

#include "IAnimationPlayer.h"

#include <string>

namespace splite
{

class SpineAnimator final : public IAnimationPlayer
{
public:
    bool Load(const std::wstring& skeletonFile, const std::wstring& atlasFile);
    bool IsRuntimeAvailable() const noexcept;

    void SetState(AnimationState state) noexcept override;
    AnimationState GetState() const noexcept override;
    void Update(float deltaSeconds) noexcept override;
    AnimationSnapshot GetSnapshot() const noexcept override;

private:
    AnimationState state_ = AnimationState::Idle;
};

} // namespace splite
