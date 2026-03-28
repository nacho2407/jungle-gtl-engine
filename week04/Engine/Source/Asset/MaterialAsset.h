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

    // void SetDiffuseTexture(UTexture2DAsset* NewTexture);

    // 직렬화(저장)할 때 참조 중인 텍스처 목록을 뽑아볼 수 있도록 제공
    TArray<UTexture2DAsset*> GetReferencedTextures() const;

  private:
    std::shared_ptr<FMaterialResource> Resource;
    UTexture2DAsset*                   DiffuseTextureAsset = nullptr;
    UTexture2DAsset*                   AmbientTextureAsset = nullptr;
    UTexture2DAsset*                   SpecularTextureAsset = nullptr;
    UTexture2DAsset*                   NormalTextureAsset = nullptr;
};
