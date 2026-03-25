#include "UUIDComponent.h"

#include "Engine/Component/Core/PrimitiveComponent.h"
#include "Engine/Game/Actor.h"

namespace Engine::Component
{
    UUUIDComponent::UUUIDComponent()
    {
        SetFontPath("Font\\JetBrainsMono\\JetBrainsMono_Medium.Font");
        SetColor(FColor::White());
        SetBillboard(true);
    }

    void UUUIDComponent::RefreshFromOwner()
    {
        if (AActor* OwnerActor = GetOwnerActor())
        {
            SetText("UUID: " + std::to_string(OwnerActor->UUID));
        }
    }

    FMatrix UUUIDComponent::GetRenderPlacementWorld(const AActor& InOwnerActor) const
    {
        return FMatrix::MakeTranslation(ComputeWorldAnchor(InOwnerActor));
    }

    FVector UUUIDComponent::GetRenderPlacementOffset(const AActor& InOwnerActor) const
    {
        (void)InOwnerActor;
        return FVector::ZeroVector;
    }

    FVector UUUIDComponent::ComputeWorldAnchor(const AActor& InOwnerActor) const
    {
        const auto* PrimitiveRoot =
            dynamic_cast<const UPrimitiveComponent*>(InOwnerActor.GetRootComponent());
        if (PrimitiveRoot != nullptr)
        {
            const Geometry::FAABB& WorldAABB = PrimitiveRoot->GetWorldAABB();
            FVector Anchor = (WorldAABB.Min + WorldAABB.Max) * 0.5f;
            Anchor.Z = WorldAABB.Max.Z + DefaultVerticalPadding;
            return Anchor;
        }

        FVector Anchor = InOwnerActor.GetWorldMatrix().GetOrigin();
        Anchor.Z += DefaultVerticalPadding;
        return Anchor;
    }

    REGISTER_CLASS(Engine::Component, UUUIDComponent)
} // namespace Engine::Component
