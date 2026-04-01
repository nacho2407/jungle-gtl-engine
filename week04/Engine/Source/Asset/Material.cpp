#include "Asset/Material.h"
#include "Core/CoreMinimal.h"

namespace Engine::Asset
{
    void UMaterial::Initialize(const FSourceRecord& InSource, const FMaterial& InMaterial,
                               const FString& InMaterialName)
    {
        InitializeAssetMetadata(InSource);
        MaterialData = InMaterial;
        if (!InMaterialName.empty())
        {
            MaterialName = InMaterialName;
        }
        else
        {
            MaterialName = GetAssetName();
            if (MaterialName.empty())
                MaterialName = "NoName";
        }
    }

    void UMaterial::SetTextureDependency(EMaterialTextureSlot Slot, UTexture2DAsset* InTexture)
    {
        if (InTexture)
        {
            ReferencedTextures.insert_or_assign(Slot, InTexture);
        }
        else
        {
            ReferencedTextures.erase(Slot);
        }
    }

    UTexture2DAsset* UMaterial::GetTextureDependency(EMaterialTextureSlot Slot) const
    {
        auto It = ReferencedTextures.find(Slot);
        return (It != ReferencedTextures.end()) ? It->second : nullptr;
    }

    bool UMaterial::HasTextureDependency(EMaterialTextureSlot Slot) const
    {
        return ReferencedTextures.find(Slot) != ReferencedTextures.end();
    }

    REGISTER_CLASS(Engine::Asset, UMaterial)
} // namespace Engine::Asset
