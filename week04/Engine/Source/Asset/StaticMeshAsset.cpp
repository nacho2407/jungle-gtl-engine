#include "Core/CoreMinimal.h"
#include "StaticMeshAsset.h"

void UStaticMeshAsset::Initialize(const FSourceRecord&                 InSource,
                                  std::shared_ptr<FStaticMeshResource> InResource)
{
    InitializeAssetMetadata(InSource);
    Resource = std::move(InResource);
}

REGISTER_CLASS(, UStaticMeshAsset)