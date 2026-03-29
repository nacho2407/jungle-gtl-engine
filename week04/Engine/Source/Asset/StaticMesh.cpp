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

    void UStaticMesh::InitializeMaterialSlots(uint32 NumSlots)
    {
        Materials.resize(NumSlots, nullptr);
    }

    UMaterialInterface* UStaticMesh::GetMaterial(uint32 Index) const
    {
        // 배열 범위를 벗어나지 않도록 안전 검사
        if (Index < Materials.size())
        {
            return Materials[Index];
        }
        return nullptr;
    }

    void UStaticMesh::SetMaterial(uint32 Index, UMaterialInterface* InMaterial)
    {
        if (Index < Materials.size())
        {
            Materials[Index] = InMaterial;
        }
    }

    /*void UStaticMesh::AddMaterialDependency(UMaterialAsset* InMaterial)
    {
        if (InMaterial && !HasMaterialDependency(InMaterial))
        {
            ReferencedMaterials.push_back(InMaterial);
        }
    }

    bool UStaticMesh::HasMaterialDependency(const UMaterialAsset* InMaterial) const
    {
        auto It = std::find(ReferencedMaterials.begin(), ReferencedMaterials.end(), InMaterial);
        return It != ReferencedMaterials.end();
    }*/

    REGISTER_CLASS(Engine::Asset, UStaticMesh)
} // namespace Engine::Asset