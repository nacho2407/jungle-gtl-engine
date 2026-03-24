#pragma once

#include "Engine/Component/Mesh/QuadComponent.h"
#include "Renderer/RenderAsset/TextureResource.h"

namespace Engine::Component
{
    class ENGINE_API USpriteComponent : public UQuadComponent
    {
        DECLARE_RTTI(USpriteComponent, UQuadComponent)
      public:
        USpriteComponent() = default;
        ~USpriteComponent() override = default;

        const FTextureResource* GetTextureResource() const { return TextureResource; }
        FTextureResource*       GetTextureResource() { return TextureResource; }
        void                    SetTextureResource(FTextureResource* InTextureResource);

        bool GetBillboard() const { return bBillboard; }
        void SetBillboard(bool bInBillboard);

        const FVector& GetBillboardOffset() const { return BillboardOffset; }
        void           SetBillboardOffset(const FVector& InBillboardOffset);

      protected:
        FTextureResource* TextureResource = nullptr;

        bool    bBillboard = false;
        FVector BillboardOffset = FVector(0.0f, 0.0f, 0.0f);
    };
} // namespace Engine::Component
