#include "Core/CoreMinimal.h"
#include "StaticMesh.h"

namespace Engine::Asset
{
    void UStaticMesh::Initialize(const FSourceRecord&                 InSource,
                                 std::shared_ptr<FStaticMeshResource> InResource)
    {
        InitializeAssetMetadata(InSource);
        RenderResource = std::move(InResource);
    }

    void UStaticMesh::InitializeMaterialSlots(uint32 NumSlots) { MaterialSlots.resize(NumSlots); }

    const FMaterialSlot* UStaticMesh::GetMaterialSlot(uint32 Index) const
    {
        if (Index < MaterialSlots.size())
        {
            return &MaterialSlots[Index];
        }
        return nullptr;
    }

    FMaterialSlot* UStaticMesh::GetMaterialSlot(uint32 Index)
    {
        if (Index < MaterialSlots.size())
        {
            return &MaterialSlots[Index];
        }
        return nullptr;
    }

    UMaterialInterface* UStaticMesh::GetMaterial(uint32 Index) const
    {
        if (Index < MaterialSlots.size())
        {
            return MaterialSlots[Index].Material;
        }
        return nullptr;
    }

    const FString& UStaticMesh::GetSubMaterialName(uint32 Index) const
    {
        static const FString EmptyString = "";

        if (Index < MaterialSlots.size())
        {
            return MaterialSlots[Index].SubMaterialName;
        }
        return EmptyString;
    }

    void UStaticMesh::SetMaterialSlot(uint32 Index, UMaterialInterface* InMaterial,
                                      const FString& InSubMaterialName)
    {
        if (Index < MaterialSlots.size())
        {
            MaterialSlots[Index].Material = InMaterial;
            MaterialSlots[Index].SubMaterialName = InSubMaterialName;
        }
    }

    REGISTER_CLASS(Engine::Asset, UStaticMesh)
} // namespace Engine::Asset