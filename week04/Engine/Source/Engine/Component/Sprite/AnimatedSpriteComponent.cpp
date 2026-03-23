#include "AnimatedSpriteComponent.h"

namespace Engine::Component
{
    void UAnimatedSpriteComponent::SetLooping(bool bInLoop)
    {
        bLoop = bInLoop;
    }

    void UAnimatedSpriteComponent::SetPlayRate(float InPlayRate)
    {
        PlayRate = (InPlayRate >= 0.0f) ? InPlayRate : 0.0f;
    }

    void UAnimatedSpriteComponent::SetStartFrame(int32 InStartFrame)
    {
        StartFrame = (InStartFrame >= 0) ? InStartFrame : 0;
        if (EndFrame < StartFrame)
        {
            EndFrame = StartFrame;
        }
        SetFrameIndex(FrameIndex);
    }

    void UAnimatedSpriteComponent::SetEndFrame(int32 InEndFrame)
    {
        EndFrame = (InEndFrame >= 0) ? InEndFrame : 0;
        if (EndFrame < StartFrame)
        {
            StartFrame = EndFrame;
        }
        SetFrameIndex(FrameIndex);
    }

    void UAnimatedSpriteComponent::SetFrameRange(int32 InStartFrame, int32 InEndFrame)
    {
        StartFrame = (InStartFrame >= 0) ? InStartFrame : 0;
        EndFrame = (InEndFrame >= StartFrame) ? InEndFrame : StartFrame;
        SetFrameIndex(FrameIndex);
    }

    void UAnimatedSpriteComponent::ResetAnimation()
    {
        CurrentTime = 0.0f;
        SetFrameIndex(StartFrame);
    }

    void UAnimatedSpriteComponent::Update(float DeltaTime)
    {
        USubUVComponent::Update(DeltaTime);

        const int32 TotalFrameCount = GetFrameCount();
        if (TotalFrameCount <= 0 || PlayRate <= 0.0f)
        {
            return;
        }

        int32 ClampedStartFrame = (StartFrame >= 0) ? StartFrame : 0;
        int32 ClampedEndFrame = (EndFrame >= ClampedStartFrame) ? EndFrame : ClampedStartFrame;
        if (ClampedStartFrame >= TotalFrameCount)
        {
            ClampedStartFrame = TotalFrameCount - 1;
        }
        if (ClampedEndFrame >= TotalFrameCount)
        {
            ClampedEndFrame = TotalFrameCount - 1;
        }

        const int32 AnimatedFrameCount = (ClampedEndFrame - ClampedStartFrame) + 1;
        if (AnimatedFrameCount <= 0)
        {
            return;
        }

        CurrentTime += DeltaTime * PlayRate;
        int32 RelativeFrame = static_cast<int32>(CurrentTime);

        if (bLoop)
        {
            RelativeFrame %= AnimatedFrameCount;
        }
        else if (RelativeFrame >= AnimatedFrameCount)
        {
            RelativeFrame = AnimatedFrameCount - 1;
        }

        SetFrameIndex(ClampedStartFrame + RelativeFrame);
    }

    REGISTER_CLASS(Engine::Component, UAnimatedSpriteComponent)
} // namespace Engine::Component
