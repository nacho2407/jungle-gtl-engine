#include "Asset/StaticMesh.h"
#include "Asset/Asset.h"
#include "Asset/AssetManager.h"
#include "CoreUObject/Object.h"


namespace Engine::Asset
{
    void UStaticMesh::Initialize(const FSourceRecord& InSource,
                                 std::shared_ptr<FStaticMesh> InResource)
    {
        InitializeAssetMetadata(InSource);
        MeshResource = InResource;
    }

    void UStaticMesh::InitializeMaterialSlots(uint32 NumSlots)
    {
        MaterialSlots.resize(NumSlots, nullptr);
    }

    const UMaterialInterface* UStaticMesh::GetMaterial(uint32 Index) const
    {
        return (Index < MaterialSlots.size()) ? MaterialSlots[Index] : nullptr;
    }

    void UStaticMesh::SetMaterialSlot(uint32 Index, UMaterialInterface* InMaterial)
    {
        if (Index < MaterialSlots.size())
        {
            MaterialSlots[Index] = InMaterial;
        }
    }

    REGISTER_CLASS(Engine::Asset, UStaticMesh)
} // namespace Engine::Asset
