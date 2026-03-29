#pragma once

#include "MaterialInterface.h"
#include "Texture2DAsset.h"
#include <memory>

struct FMaterialResource;
struct FMaterialData;

namespace Engine::Asset
{
    class ENGINE_API UMaterialAsset : public UMaterialInterface
    {
        DECLARE_RTTI(UMaterialAsset, UMaterialInterface)
      public:
        UMaterialAsset() = default;
        ~UMaterialAsset() = default;

        void                         Initialize(const FSourceRecord&               InSource,
                                                std::shared_ptr<FMaterialResource> InResource);
        virtual const FMaterialData* GetMaterialData(const FString& SubMaterialName) const override;

        // FMaterialResource* GetResource() const { return Resource.get(); }

        TArray<UTexture2DAsset*>&       GetReferencedTextures() { return ReferencedTextures; }
        const TArray<UTexture2DAsset*>& GetReferencedTextures() const { return ReferencedTextures; }
        void                            AddTextureDependency(UTexture2DAsset* InTexture);
        bool HasTextureDependency(const UTexture2DAsset* InTexture) const;

      private:
        std::shared_ptr<FMaterialResource> Resource;
        TArray<UTexture2DAsset*>           ReferencedTextures;
    };
} // namespace Engine::Asset
