#include "SpineAnimator.h"

namespace splite
{

bool SpineAnimator::Load(const std::wstring&, const std::wstring&)
{
    // Runtime 接入后，这里负责加载 atlas 与 json/skel，并创建 AnimationState。
    return false;
}

bool SpineAnimator::IsRuntimeAvailable() const noexcept
{
    return false;
}

void SpineAnimator::SetState(AnimationState state) noexcept
{
    state_ = state;
}

AnimationState SpineAnimator::GetState() const noexcept
{
    return state_;
}

void SpineAnimator::Update(float) noexcept
{
}

AnimationSnapshot SpineAnimator::GetSnapshot() const noexcept
{
    return {};
}

} // namespace splite
