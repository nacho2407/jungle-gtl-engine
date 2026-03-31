#include "Asset/Material.h"
#include "Asset/Asset.h"
#include "Asset/AssetManager.h"
#include "CoreUObject/Object.h"
#include "Renderer/RenderAsset/MaterialResource.h"

namespace Engine::Asset
{
    void UMaterial::Initialize(const FSourceRecord& InSource, std::shared_ptr<::FMaterialResource> InResource)
    {
        InitializeAssetMetadata(InSource);
        Resource = InResource;
        // SetPath(InSource.NormalizedPath);
    }

    const FMaterialData* UMaterial::GetMaterialData(const FString& SubMaterialName) const
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

    void UMaterial::AddTextureDependency(UTexture2DAsset* InTexture)
    {
        if (InTexture && !HasTextureDependency(InTexture))
        {
            ReferencedTextures.push_back(InTexture);
        }
    }

    bool UMaterial::HasTextureDependency(const UTexture2DAsset* InTexture) const
    {
        for (const auto* Tex : ReferencedTextures)
        {
            if (Tex == InTexture) return true;
        }
        return false;
    }

    REGISTER_CLASS(Engine::Asset, UMaterial)
} // namespace Engine::Asset
