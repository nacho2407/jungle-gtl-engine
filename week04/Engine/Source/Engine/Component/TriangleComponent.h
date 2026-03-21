#pragma once

#include <Core/CoreMinimal.h>
#include "PrimitiveComponent.h"

namespace Engine::Component
{
    class ENGINE_API UTriangleComp : public UPrimitiveComponent
    {
        DECLARE_RTTI(UTriangleComp, UPrimitiveComponent)

      public:
        UTriangleComp();
        virtual ~UTriangleComp() override;
        //virtual void Update(float DeltaTime) override;
    };

} // namespace Engine::Component