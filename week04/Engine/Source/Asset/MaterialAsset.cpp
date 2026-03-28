#include "Core/CoreMinimal.h"
#include "MaterialAsset.h"

void UMaterialAsset::Initialize(const FSourceRecord&               InSource,
                                std::shared_ptr<FMaterialResource> InResource)
{
    InitializeAssetMetadata(InSource);
    Resource = std::move(InResource);
}

void UMaterialAsset::AddTextureDependency(UTexture2DAsset* InTexture)
{
    if (InTexture && !HasTextureDependency(InTexture))
    {
        ReferencedTextures.push_back(InTexture);
    }
}

bool UMaterialAsset::HasTextureDependency(const UTexture2DAsset* InTexture) const
{
    auto It = std::find(ReferencedTextures.begin(), ReferencedTextures.end(), InTexture);
    return It != ReferencedTextures.end();
}

REGISTER_CLASS(, UMaterialAsset)
