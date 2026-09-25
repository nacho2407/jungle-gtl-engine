#pragma once

#include "Engine/Component/Core/SceneComponent.h"

namespace Engine::Component
{
    class ENGINE_API UUnknownComponent : public USceneComponent
    {
        DECLARE_RTTI(UUnknownComponent, USceneComponent)

      public:
        UUnknownComponent() = default;
        ~UUnknownComponent() override = default;

        void SetOriginalTypeName(const FString& InTypeName) { OriginalTypeName = InTypeName; }
        const FString& GetOriginalTypeName() const { return OriginalTypeName; }

        void SetSerializedPayload(const FString& InPayload) { SerializedPayload = InPayload; }
        const FString& GetSerializedPayload() const { return SerializedPayload; }

      private:
        FString OriginalTypeName;
        FString SerializedPayload;
    };
} // namespace Engine::Component
