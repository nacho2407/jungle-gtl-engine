#pragma once

#include "PrimitiveComponent.h"

namespace Engine::Component
{
    class UPlaneComp : public UPrimitiveComponent
    {
        DECLARE_RTTI(UPlaneComp, UPrimitiveComponent)
      public:
        UPlaneComp();
        virtual ~UPlaneComp() override;
        virtual void Update(float DeltaTime) override;
    };
} // namespace Engine::Component