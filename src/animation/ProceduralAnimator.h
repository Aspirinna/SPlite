#pragma once

#include "IAnimationPlayer.h"

namespace splite
{

// 无外部素材也可运行的占位动画器。
// 它让静态 PNG 轻微呼吸浮动，同时验证应用层的时间推进和状态切换。
class ProceduralAnimator final : public IAnimationPlayer
{
public:
    void SetState(AnimationState state) noexcept override;
    AnimationState GetState() const noexcept override;
    void Update(float deltaSeconds) noexcept override;
    AnimationSnapshot GetSnapshot() const noexcept override;

private:
    AnimationState state_ = AnimationState::Idle;
    float stateTime_ = 0.0f;
    AnimationSnapshot snapshot_{};
};

} // namespace splite
