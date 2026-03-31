#pragma once

#include "Asset/MaterialInterface.h"
#include "Asset/Texture2DAsset.h"
#include "Source/Renderer/RenderAsset/MaterialResource.h"
#include <memory>

struct FMaterialResource;
struct FMaterialData;

namespace Engine::Asset
{
    /**
     * @brief 빛과 표면의 상호작용을 결정하는 물리 기반 머티리얼 에셋입니다.
     * OBJ와 연동된 MTL 파일의 모든 재질 정보를 관리합니다.
     */
    class ENGINE_API UMaterial : public UMaterialInterface
    {
        DECLARE_RTTI(UMaterial, UMaterialInterface)
      public:
        UMaterial() = default;
        virtual ~UMaterial() override = default;

        void Initialize(const FSourceRecord& InSource, std::shared_ptr<::FMaterialResource> InResource);
        virtual const FMaterialData* GetMaterialData(const FString& SubMaterialName) const override;

        TArray<UTexture2DAsset*>&       GetReferencedTextures() { return ReferencedTextures; }
        const TArray<UTexture2DAsset*>& GetReferencedTextures() const { return ReferencedTextures; }
        void                            AddTextureDependency(UTexture2DAsset* InTexture);
        bool HasTextureDependency(const UTexture2DAsset* InTexture) const;

      private:
        std::shared_ptr<::FMaterialResource> Resource;
        TArray<UTexture2DAsset*>           ReferencedTextures;
    };
} // namespace Engine::Asset
