#include "Asset/Material.h"
#include "Asset/Asset.h"
#include "Asset/AssetManager.h"
#include "CoreUObject/Object.h"
#include "Renderer/RenderAsset/MaterialResource.h"
#include <filesystem>

namespace Engine::Asset
{
    void UMaterial::Initialize(const FSourceRecord& InSource, std::shared_ptr<::FMaterialResource> InResource)
    {
        InitializeAssetMetadata(InSource);
        Resource = InResource;

        // 경로에서 파일 이름 추출 (예: "C:/Data/stone.mtl" -> "stone.mtl")
        std::filesystem::path FilePath(InSource.NormalizedPath);
        FString ExtractedName = FilePath.filename().string().c_str();

        // 에셋 이름 및 UObject 이름 설정
        SetAssetName(ExtractedName);
        Name = ExtractedName.c_str();
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
