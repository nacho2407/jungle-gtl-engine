#pragma once

#include "SubUVComponent.h"

namespace Engine::Component
{
    class ENGINE_API UAnimatedSpriteComponent : public USubUVComponent
    {
        DECLARE_RTTI(UAnimatedSpriteComponent, USubUVComponent)
      public:
        UAnimatedSpriteComponent() = default;
        ~UAnimatedSpriteComponent() override = default;

        bool  IsLooping() const { return bLoop; }
        float GetPlayRate() const { return PlayRate; }
        int32 GetStartFrame() const { return StartFrame; }
        int32 GetEndFrame() const { return EndFrame; }
        float GetCurrentTime() const { return CurrentTime; }

        void SetLooping(bool bInLoop);
        void SetPlayRate(float InPlayRate);
        void SetStartFrame(int32 InStartFrame);
        void SetEndFrame(int32 InEndFrame);
        void SetFrameRange(int32 InStartFrame, int32 InEndFrame);
        void ResetAnimation();

        void Update(float DeltaTime) override;

      protected:
        bool  bLoop = true;
        float PlayRate = 1.0f;
        int32 StartFrame = 0;
        int32 EndFrame = 0;
        float CurrentTime = 0.0f;
    };
} // namespace Engine::Component
