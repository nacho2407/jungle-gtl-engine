#pragma once

#include "SubUVComponent.h"

namespace Engine::Component
{
    class ENGINE_API USubUVAnimatedComponent : public USubUVComponent
    {
        DECLARE_RTTI(USubUVAnimatedComponent, USubUVComponent)
      public:
        USubUVAnimatedComponent() = default;
        ~USubUVAnimatedComponent() override = default;

        void Update(float DeltaTime) override;
        void DescribeProperties(FComponentPropertyBuilder& Builder) override;

        float GetAnimationFPS() const { return AnimationFPS; }
        void  SetAnimationFPS(float InAnimationFPS);

        bool IsLooping() const { return bLoopFlag; }
        void SetLooping(bool bInLoopFlag) { bLoopFlag = bInLoopFlag; }

      protected:
        float AnimationFPS = 30.0f;
        float AnimationTimeAccumulator = 0.0f;
        bool  bLoopFlag = true;
    };
} // namespace Engine::Component
