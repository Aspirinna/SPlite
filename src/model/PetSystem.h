#pragma once

// 模块：多角色实例系统
// 职责：保存每只桌宠的独立状态、推进动画，并处理单只角色的拖动。
//       纹理和 D3D 资源不属于角色实例，由渲染器统一共享。

#include "../animation/ProceduralAnimator.h"
#include "../graphics/RendererD3D11.h"

#include <string>
#include <vector>

namespace splite
{

struct PetInstance
{
    std::wstring id;
    float x = 0.0f;
    float y = 0.0f;
    float scale = 1.0f;
    ProceduralAnimator animator;
};

class PetSystem
{
public:
    void AddPet(std::wstring id, float x, float y, float scale = 1.0f);
    void Update(float deltaSeconds);
    std::vector<SpriteTransform> BuildRenderTransforms() const;

    bool BeginDrag(int petIndex, int clientX, int clientY);
    void DragTo(int clientX, int clientY, int clientWidth, int clientHeight);
    void EndDrag();
    bool IsDragging() const noexcept;

private:
    std::vector<PetInstance> pets_;
    int draggedPet_ = -1;
    float dragOffsetX_ = 0.0f;
    float dragOffsetY_ = 0.0f;
};

} // namespace splite
