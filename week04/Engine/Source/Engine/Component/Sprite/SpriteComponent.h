#pragma once

#include "Engine/Component/Core/PrimitiveComponent.h"
#include "Renderer/RenderAsset/TextureResource.h"
#include "Renderer/Types/BasicMeshType.h" // EBasicMeshType 정의 포함

namespace Engine::Component
{
    /**
     * @brief 2D 스프라이트를 렌더링하는 컴포넌트입니다.
     */
    class ENGINE_API USpriteComponent : public UPrimitiveComponent
    {
        DECLARE_RTTI(USpriteComponent, UPrimitiveComponent)
      public:
        USpriteComponent() = default;
        ~USpriteComponent() override = default;

        const FTextureResource* GetTextureResource() const { return TextureResource; }
        FTextureResource*       GetTextureResource() { return TextureResource; }
        void                    SetTextureResource(FTextureResource* InTextureResource);

        const FString& GetTexturePath() const { return TexturePath; }
        void           SetTexturePath(const FString& InPath);

        bool GetBillboard() const { return bBillboard; }
        void SetBillboard(bool bInBillboard);

        const FVector& GetBillboardOffset() const { return BillboardOffset; }
        void           SetBillboardOffset(const FVector& InBillboardOffset);

        // UPrimitiveComponent/USceneComponent 인터페이스 구현
        bool GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const override;
        void DescribeProperties(class FComponentPropertyBuilder& Builder) override;
        void ResolveAssetReferences(class UAssetManager* InAssetManager) override;

        /** 스프라이트 계열은 기본적으로 Quad 타입을 반환하도록 설정 */
        EBasicMeshType GetBasicMeshType() const override { return EBasicMeshType::Quad; }

      protected:
        Geometry::FAABB GetLocalAABB() const override;

      protected:
        FTextureResource* TextureResource = nullptr;
        FString           TexturePath = "";
        bool              bBillboard = true;
        FVector           BillboardOffset = FVector::ZeroVector;
    };
} // namespace Engine::Component
