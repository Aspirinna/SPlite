#include "ProceduralAnimator.h"

#include <algorithm>
#include <cmath>

namespace splite
{

void ProceduralAnimator::SetState(AnimationState state) noexcept
{
    if (state_ != state)
    {
        state_ = state;
        stateTime_ = 0.0f;
    }
}

AnimationState ProceduralAnimator::GetState() const noexcept
{
    return state_;
}

void ProceduralAnimator::Update(float deltaSeconds) noexcept
{
    // 防止调试器暂停后一次推进数秒，造成动画瞬移。
    stateTime_ += std::clamp(deltaSeconds, 0.0f, 0.1f);
    snapshot_ = {};

    switch (state_)
    {
    case AnimationState::Idle:
        snapshot_.offsetY = std::sin(stateTime_ * 2.0f) * 3.0f;
        snapshot_.scale = 1.0f + std::sin(stateTime_ * 2.0f) * 0.01f;
        break;
    case AnimationState::Interact:
        snapshot_.scale = 1.0f + std::sin(stateTime_ * 12.0f) * 0.05f;
        if (stateTime_ >= 0.45f)
        {
            SetState(AnimationState::Idle);
        }
        break;
    case AnimationState::Drag:
        snapshot_.offsetY = -4.0f;
        snapshot_.scale = 1.03f;
        break;
    case AnimationState::Sleep:
        snapshot_.offsetY = 3.0f;
        snapshot_.scale = 0.98f;
        snapshot_.opacity = 0.92f;
        break;
    }
}

AnimationSnapshot ProceduralAnimator::GetSnapshot() const noexcept
{
    return snapshot_;
}

} // namespace splite
