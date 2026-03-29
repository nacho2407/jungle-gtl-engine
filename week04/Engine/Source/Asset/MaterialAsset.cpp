#include "Core/CoreMinimal.h"
#include "MaterialAsset.h"
#include "Renderer/RenderAsset/MaterialResource.h"

namespace Engine::Asset
{
    void UMaterialAsset::Initialize(const FSourceRecord&               InSource,
                                    std::shared_ptr<FMaterialResource> InResource)
    {
        InitializeAssetMetadata(InSource);
        Resource = std::move(InResource);
    }

    const FMaterialData*
    UMaterialAsset::GetMaterialData(const FString& SubMaterialName) const
    {
        if (Resource)
        {
            auto It = Resource->Materials.find(SubMaterialName);
            if (It != Resource->Materials.end())
            {
                return &It->second;
            }
        }
        return nullptr;
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

    REGISTER_CLASS(Engine::Asset, UMaterialAsset)
}

