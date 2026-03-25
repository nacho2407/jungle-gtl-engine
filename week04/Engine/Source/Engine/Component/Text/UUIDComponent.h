#pragma once

#include "AtlasTextComponent.h"

namespace Engine::Component
{
    class ENGINE_API UUUIDComponent : public UAtlasTextComponent
    {
        DECLARE_RTTI(UUUIDComponent, UAtlasTextComponent)

      public:
        static constexpr float DefaultVerticalPadding = 1.0f;

      public:
        UUUIDComponent();
        ~UUUIDComponent() override = default;

        void    RefreshFromOwner();
        FMatrix GetRenderPlacementWorld(const AActor& InOwnerActor) const override;
        FVector GetRenderPlacementOffset(const AActor& InOwnerActor) const override;

        bool ShouldSerializeInScene() const override { return false; }
        bool ShouldShowInDetailsTree() const override { return false; }

      private:
        FVector ComputeWorldAnchor(const AActor& InOwnerActor) const;
    };
} // namespace Engine::Component
