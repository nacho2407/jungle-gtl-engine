#include "Asset/StaticMesh.h"
#include "Asset/Asset.h"
#include "Asset/AssetManager.h"
#include "CoreUObject/Object.h"

#include <filesystem>

namespace Engine::Asset
{
    void UStaticMesh::Initialize(const FSourceRecord& InSource, std::shared_ptr<FStaticMeshResource> InResource)
    {
        InitializeAssetMetadata(InSource);
        RenderResource = InResource;

        // 경로에서 파일 이름 추출
        std::filesystem::path FilePath(InSource.NormalizedPath);
        FString ExtractedName = FilePath.filename().string().c_str();
        SetAssetName(ExtractedName);
        Name = ExtractedName.c_str();
    }

    void UStaticMesh::InitializeMaterialSlots(uint32 NumSlots)
    {
        MaterialSlots.resize(NumSlots);
    }

    const FMaterialSlot* UStaticMesh::GetMaterialSlot(uint32 Index) const
    {
        return (Index < MaterialSlots.size()) ? &MaterialSlots[Index] : nullptr;
    }

    FMaterialSlot* UStaticMesh::GetMaterialSlot(uint32 Index)
    {
        return (Index < MaterialSlots.size()) ? &MaterialSlots[Index] : nullptr;
    }

    UMaterialInterface* UStaticMesh::GetMaterial(uint32 Index) const
    {
        return (Index < MaterialSlots.size()) ? MaterialSlots[Index].Material : nullptr;
    }

    const FString& UStaticMesh::GetSubMaterialName(uint32 Index) const
    {
        static const FString EmptyString = "";
        return (Index < MaterialSlots.size()) ? MaterialSlots[Index].SubMaterialName : EmptyString;
    }

    void UStaticMesh::SetMaterialSlot(uint32 Index, UMaterialInterface* InMaterial, const FString& InSubMaterialName)
    {
        if (Index < MaterialSlots.size())
        {
            MaterialSlots[Index].Material = InMaterial;
            MaterialSlots[Index].SubMaterialName = InSubMaterialName;
        }
    }

    REGISTER_CLASS(Engine::Asset, UStaticMesh)
} // namespace Engine::Asset
