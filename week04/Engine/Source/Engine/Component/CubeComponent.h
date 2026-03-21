#pragma once

#include "PrimitiveComponent.h"

namespace Engine::Component
{
    class ENGINE_API UCubeComp : public UPrimitiveComponent
    {
        DECLARE_RTTI(UCubeComp, UPrimitiveComponent)
      public:
        UCubeComp() = default;
        virtual ~UCubeComp() override = default;

        virtual void Update(float DeltaTime) override;
    };
} // namespace Engine::Component