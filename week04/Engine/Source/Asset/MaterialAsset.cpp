#include "Core/CoreMinimal.h"
#include "MaterialAsset.h"

REGISTER_CLASS(, UMaterialAsset)

void UMaterialAsset::Initialize(const FSourceRecord&               InSource,
                                std::shared_ptr<FMaterialResource> InResource)
{
    InitializeAssetMetadata(InSource);
    Resource = std::move(InResource);
}
