#include "Core/CoreMinimal.h"
#include "StaticMeshAsset.h"

void UStaticMeshAsset::Initialize(const FSourceRecord&                 InSource,
                                  std::shared_ptr<FStaticMeshResource> InResource)
{
    InitializeAssetMetadata(InSource);
    Resource = std::move(InResource);
}

void UStaticMeshAsset::AddMaterialDependency(UMaterialAsset* InMaterial)
{
    if (InMaterial && !HasMaterialDependency(InMaterial))
    {
        ReferencedMaterials.push_back(InMaterial);
    }
}

bool UStaticMeshAsset::HasMaterialDependency(const UMaterialAsset* InMaterial) const
{
    auto It = std::find(ReferencedMaterials.begin(), ReferencedMaterials.end(), InMaterial);
    return It != ReferencedMaterials.end();
}

REGISTER_CLASS(, UStaticMeshAsset)
