#pragma once

// 模块：动画播放接口
// 职责：隔离应用逻辑与具体动画实现。PNG 测试动画和 Spine Runtime
//       都通过同一个快照结构向渲染层提供结果。

#include <string>

namespace splite
{

enum class AnimationState
{
    Idle,       // 默认待机
    Interact,   // 点击反馈
    Drag,       // 被用户拖动
    Sleep       // 长时间无操作
};

struct AnimationSnapshot
{
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float scale   = 1.0f;
    float opacity = 1.0f;
};

class IAnimationPlayer
{
public:
    virtual ~IAnimationPlayer() = default;

    virtual void SetState(AnimationState state) noexcept = 0;
    virtual AnimationState GetState() const noexcept = 0;
    virtual void Update(float deltaSeconds) noexcept = 0;
    virtual AnimationSnapshot GetSnapshot() const noexcept = 0;
};

} // namespace splite
