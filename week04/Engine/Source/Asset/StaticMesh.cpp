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

    void UStaticMesh::AddMaterialDependency(UMaterialAsset* InMaterial)
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
    }

    REGISTER_CLASS(Engine::Asset, UStaticMesh)
} // namespace Engine::Asset