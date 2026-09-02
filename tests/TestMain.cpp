#include "../src/animation/ProceduralAnimator.h"
#include "../src/model/PetSystem.h"

#include <cmath>
#include <iostream>

namespace
{
int g_failures = 0;

void Expect(bool condition, const wchar_t* description)
{
    if (!condition)
    {
        std::wcerr << L"[失败] " << description << L'\n';
        ++g_failures;
    }
}

void TestProceduralAnimator()
{
    splite::ProceduralAnimator animator;
    animator.Update(1.0f);
    const splite::AnimationSnapshot idle = animator.GetSnapshot();
    Expect(std::abs(idle.offsetY) <= 3.01f, L"待机浮动必须限制在 3 像素内");

    animator.SetState(splite::AnimationState::Drag);
    animator.Update(0.016f);
    const splite::AnimationSnapshot drag = animator.GetSnapshot();
    Expect(drag.offsetY == -4.0f, L"拖动状态应抬高角色");
    Expect(drag.scale > 1.0f, L"拖动状态应轻微放大角色");
}

void TestPetSystem()
{
    splite::PetSystem pets;
    pets.AddPet(L"first", 10.0f, 20.0f);
    pets.AddPet(L"second", 300.0f, 200.0f, 0.5f);

    Expect(pets.BuildRenderTransforms().size() == 2, L"应生成两个角色的渲染实例");
    Expect(!pets.BeginDrag(9, 0, 0), L"非法角色下标不得开始拖动");
    Expect(pets.BeginDrag(0, 30, 40), L"有效角色应能开始拖动");
    pets.DragTo(-100, -100, 800, 600);
    pets.EndDrag();

    const auto transforms = pets.BuildRenderTransforms();
    Expect(transforms[0].x >= 0.0f && transforms[0].y >= 0.0f,
           L"拖动后角色不得越过客户区左上边界");
}
}

int wmain()
{
    TestProceduralAnimator();
    TestPetSystem();

    if (g_failures == 0)
    {
        std::wcout << L"SPlite 核心测试全部通过。\n";
        return 0;
    }

    std::wcerr << L"共有 " << g_failures << L" 项测试失败。\n";
    return 1;
}
