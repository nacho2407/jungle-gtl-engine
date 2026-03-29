#pragma once
#include "StreamableRenderAsset.h"
#include "Asset/MaterialAsset.h"
#include "Renderer/RenderAsset/StaticMeshResource.h"
#include <memory>


namespace Engine::Asset
{
    //class UMaterialAsset;
    class UMaterialInterface;

    class ENGINE_API UStaticMesh : public UStreamableRenderAsset
    {
        DECLARE_RTTI(UStaticMesh, UStreamableRenderAsset)
      public:
        UStaticMesh() = default;
        ~UStaticMesh() = default;

        void Initialize(const FSourceRecord&                 InSource,
                        std::shared_ptr<FStaticMeshResource> InResource);

        const FStaticMeshResource* GetRenderResource() const { return RenderResource.get(); }
        FStaticMeshResource*       GetRenderResource() { return RenderResource.get(); }

        void InitializeMaterialSlots(uint32 NumSlots);
        UMaterialInterface* GetMaterial(uint32 Index) const;
        void                SetMaterial(uint32 Index, UMaterialInterface* InMaterial);
        const TArray<UMaterialInterface*>& GetMaterials() const { return Materials; }

        /*const TArray<UMaterialAsset*>& GetReferencedMaterials() const
        {
            return ReferencedMaterials;
        }
        void AddMaterialDependency(UMaterialAsset* InMaterial);
        bool HasMaterialDependency(const UMaterialAsset* InMaterial) const;*/

      private:
        std::shared_ptr<FStaticMeshResource> RenderResource;
        TArray<UMaterialInterface*>          Materials;
    };
}

