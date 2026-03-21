#pragma once

#include <Core/CoreMinimal.h>
#include "Engine/Component/PrimitiveComponent.h"
#include "Renderer/Types/BasicMeshType.h"

enum class EBasicMeshType : uint8;

namespace Engine::Component
{
    class ENGINE_API UTriangleComponent : public UPrimitiveComponent
    {
        DECLARE_RTTI(UTriangleComponent, UPrimitiveComponent)

      public:
        UTriangleComponent() = default;
        virtual ~UTriangleComponent() override = default;

        void   Update(float DeltaTime) override;
        EBasicMeshType GetBasicMeshType() const override;
    };
} // namespace Engine::Component
