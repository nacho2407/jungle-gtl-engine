#pragma once
#include "Asset.h"
#include "Renderer/RenderAsset/StaticMeshResource.h"
#include <memory>

class ENGINE_API UStaticMeshAsset : public UAsset
{
    DECLARE_RTTI(UStaticMeshAsset, UAsset)
  public:
    UStaticMeshAsset() = default;
    ~UStaticMeshAsset() = default;

    void Initialize(const FSourceRecord& InSource, std::shared_ptr<FStaticMeshResource> InResource);

    const FStaticMeshResource* GetResource() const { return Resource.get(); }
    FStaticMeshResource*       GetResource() { return Resource.get(); }

  private:
    std::shared_ptr<FStaticMeshResource> Resource;
};
