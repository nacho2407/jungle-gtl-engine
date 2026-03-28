#pragma once

#include "Asset.h"
#include "Texture2DAsset.h"
#include "Renderer/RenderAsset/MaterialResource.h"
#include <memory>

struct FMaterialResource;

class ENGINE_API UMaterialAsset : public UAsset
{
    DECLARE_RTTI(UMaterialAsset, UAsset)
  public:
    UMaterialAsset() = default;
    ~UMaterialAsset() = default;

    void Initialize(const FSourceRecord& InSource, std::shared_ptr<FMaterialResource> InResource);
    FMaterialResource* GetResource() const { return Resource.get(); }

    TArray<UTexture2DAsset*>&       GetReferencedTextures() { return ReferencedTextures; }
    const TArray<UTexture2DAsset*>& GetReferencedTextures() const { return ReferencedTextures; }
    void                            AddTextureDependency(UTexture2DAsset* InTexture);
    bool                            HasTextureDependency(const UTexture2DAsset* InTexture) const;

  private:
    std::shared_ptr<FMaterialResource> Resource;
    TArray<UTexture2DAsset*>           ReferencedTextures;
};
