#pragma once

#include "PrimitiveComponent.h"

namespace Engine::Component
{
    class ENGINE_API UCubeComp : public UPrimitiveComponent
    {
        DECLARE_RTTI(UCubeComp, UPrimitiveComponent)
      public:
        UCubeComp();
        virtual ~UCubeComp() override;
        // virtual void Update(float DeltaTime) override;
    };
} // namespace Engine::Component