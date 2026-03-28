#pragma once

#include "Asset.h"
#include "Renderer/RenderAsset/MaterialResource.h"
#include <memory>

class ENGINE_API UMaterialAsset : public UAsset
{
    DECLARE_RTTI(UMaterialAsset, UAsset)
  public:
    UMaterialAsset() = default;
    ~UMaterialAsset() = default;

    void Initialize(const FSourceRecord& InSource, std::shared_ptr<FMaterialResource> InResource);

    FMaterialResource* GetResource() const { return Resource.get(); }

  private:
    std::shared_ptr<FMaterialResource> Resource;
};
