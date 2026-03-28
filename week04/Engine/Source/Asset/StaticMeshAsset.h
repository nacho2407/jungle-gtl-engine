#pragma once
#include "Asset.h"
#include "Asset/MaterialAsset.h"
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

    const TArray<UMaterialAsset*>& GetReferencedMaterials() const { return ReferencedMaterials; }
    void                           AddMaterialDependency(UMaterialAsset* InMaterial);
    bool                           HasMaterialDependency(const UMaterialAsset* InMaterial) const;

  private:
    std::shared_ptr<FStaticMeshResource> Resource;
    TArray<UMaterialAsset*>              ReferencedMaterials;
};
