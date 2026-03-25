#include "SubUVAnimatedComponent.h"
#include "Engine/Component/Core/ComponentProperty.h"

namespace Engine::Component
{
    void USubUVAnimatedComponent::Update(float DeltaTime)
    {
        USubUVComponent::Update(DeltaTime);

        const int32 FrameCount = GetFrameCount();
        if (FrameCount <= 1 || AnimationFPS <= 0.0f || DeltaTime <= 0.0f)
        {
            return;
        }

        const float SecondsPerFrame = 1.0f / AnimationFPS;
        AnimationTimeAccumulator += DeltaTime;

        if (AnimationTimeAccumulator < SecondsPerFrame)
        {
            return;
        }

        const int32 FramesToAdvance =
            static_cast<int32>(AnimationTimeAccumulator / SecondsPerFrame);
        AnimationTimeAccumulator -= SecondsPerFrame * static_cast<float>(FramesToAdvance);

        const int32 CurrentFrame = GetFrameIndex();
        int32       NextFrameIndex = CurrentFrame + FramesToAdvance;

        if (bLoopFlag)
        {
            NextFrameIndex = NextFrameIndex % FrameCount;
        }
        else
        {
            if (NextFrameIndex >= FrameCount)
            {
                NextFrameIndex = FrameCount - 1;
                AnimationTimeAccumulator = 0.0f;
            }
        }

        SetFrameIndex(NextFrameIndex);
    }

    void USubUVAnimatedComponent::SetAnimationFPS(float InAnimationFPS)
    {
        AnimationFPS = (InAnimationFPS >= 0.0f) ? InAnimationFPS : 0.0f;
    }

    void USubUVAnimatedComponent::DescribeProperties(FComponentPropertyBuilder& Builder)
    {
        USubUVComponent::DescribeProperties(Builder);

        FComponentPropertyOptions FloatOptions;
        FloatOptions.DragSpeed = 0.1f;

        Builder.AddFloat(
            "animation_fps", L"Animation FPS", [this]() { return GetAnimationFPS(); },
            [this](float InValue) { SetAnimationFPS(InValue); }, FloatOptions);

        Builder.AddBool(
            "looping", L"Looping", [this]() { return IsLooping(); },
            [this](bool bInValue) { SetLooping(bInValue); });
    }

    REGISTER_CLASS(Engine::Component, USubUVAnimatedComponent)
} // namespace Engine::Component