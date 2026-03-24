#include "SpriteComponent.h"

namespace Engine::Component
{
    void USpriteComponent::SetTextureResource(FTextureResource* InTextureResource)
    {
        TextureResource = InTextureResource;
    }

    void USpriteComponent::SetBillboard(bool bInBillboard) { bBillboard = bInBillboard; }

    void USpriteComponent::SetBillboardOffset(const FVector& InBillboardOffset)
    {
        BillboardOffset = InBillboardOffset;
    }

    REGISTER_CLASS(Engine::Component, USpriteComponent)
} // namespace Engine::Component
