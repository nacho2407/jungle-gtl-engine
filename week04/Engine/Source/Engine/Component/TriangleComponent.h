#pragma once

#include "Core/CoreMinimal.h"
#include "PrimitiveComponent.h"

namespace Engine::Component
{
    class ENGINE_API UTriangleComp : public UPrimitiveComponent
    {
        DECLARE_RTTI(UTriangleComp, UPrimitiveComponent)

      public:
        UTriangleComp() = default;
        virtual ~UTriangleComp() override = default;

        virtual void Update(float DeltaTime) override;
    };
} // namespace Engine::Component
