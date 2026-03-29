#pragma once
#include "StreamableRenderAsset.h"
#include "Asset/MaterialAsset.h"
#include "Renderer/RenderAsset/StaticMeshResource.h"
#include <memory>


namespace Engine::Asset
{
    class UMaterialAsset;

    class ENGINE_API UStaticMesh : public UStreamableRenderAsset
    {
        DECLARE_RTTI(UStaticMesh, UStreamableRenderAsset)
      public:
        UStaticMesh() = default;
        ~UStaticMesh() = default;

        void Initialize(const FSourceRecord&                 InSource,
                        std::shared_ptr<FStaticMeshResource> InResource);

        const FStaticMeshResource* GetResource() const { return RenderResource.get(); }
        FStaticMeshResource*       GetResource() { return RenderResource.get(); }

        const TArray<UMaterialAsset*>& GetReferencedMaterials() const
        {
            return ReferencedMaterials;
        }
        void AddMaterialDependency(UMaterialAsset* InMaterial);
        bool HasMaterialDependency(const UMaterialAsset* InMaterial) const;

      private:
        std::shared_ptr<FStaticMeshResource> RenderResource;
        TArray<UMaterialAsset*>              ReferencedMaterials;
    };
}
