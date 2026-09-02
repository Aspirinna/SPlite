#include "PetSystem.h"

#include <algorithm>
#include <utility>

namespace splite
{

void PetSystem::AddPet(std::wstring id, float x, float y, float scale)
{
    pets_.push_back(PetInstance{ std::move(id), x, y, scale, {} });
}

void PetSystem::Update(float deltaSeconds)
{
    for (PetInstance& pet : pets_)
    {
        pet.animator.Update(deltaSeconds);
    }
}

std::vector<SpriteTransform> PetSystem::BuildRenderTransforms() const
{
    std::vector<SpriteTransform> transforms;
    transforms.reserve(pets_.size());
    for (const PetInstance& pet : pets_)
    {
        const AnimationSnapshot animation = pet.animator.GetSnapshot();
        transforms.push_back({
            pet.x + animation.offsetX,
            pet.y + animation.offsetY,
            pet.scale * animation.scale,
            animation.opacity
        });
    }
    return transforms;
}

bool PetSystem::BeginDrag(int petIndex, int clientX, int clientY)
{
    if (petIndex < 0 || petIndex >= static_cast<int>(pets_.size()))
    {
        return false;
    }

    draggedPet_ = petIndex;
    PetInstance& pet = pets_[petIndex];
    dragOffsetX_ = static_cast<float>(clientX) - pet.x;
    dragOffsetY_ = static_cast<float>(clientY) - pet.y;
    pet.animator.SetState(AnimationState::Drag);
    return true;
}

void PetSystem::DragTo(int clientX, int clientY, int clientWidth, int clientHeight)
{
    if (draggedPet_ < 0)
    {
        return;
    }

    PetInstance& pet = pets_[draggedPet_];
    const float petSize = 256.0f * pet.scale;
    pet.x = std::clamp(static_cast<float>(clientX) - dragOffsetX_, 0.0f,
                       (std::max)(0.0f, clientWidth - petSize));
    pet.y = std::clamp(static_cast<float>(clientY) - dragOffsetY_, 0.0f,
                       (std::max)(0.0f, clientHeight - petSize));
}

void PetSystem::EndDrag()
{
    if (draggedPet_ >= 0)
    {
        pets_[draggedPet_].animator.SetState(AnimationState::Idle);
        draggedPet_ = -1;
    }
}

bool PetSystem::IsDragging() const noexcept
{
    return draggedPet_ >= 0;
}

} // namespace splite
