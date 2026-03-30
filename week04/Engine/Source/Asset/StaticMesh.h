#pragma once
#include "StreamableRenderAsset.h"
#include "Asset/MaterialAsset.h"
#include "Renderer/RenderAsset/StaticMeshResource.h"
#include <memory>

namespace Engine::Asset
{
    class UMaterialInterface;

    struct FMaterialSlot
    {
        UMaterialInterface* Material = nullptr;
        FString             SubMaterialName;
    };

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

        const FMaterialSlot* GetMaterialSlot(uint32 Index) const;
        FMaterialSlot*       GetMaterialSlot(uint32 Index);

        UMaterialInterface* GetMaterial(uint32 Index) const;
        const FString&      GetSubMaterialName(uint32 Index) const;

        void SetMaterialSlot(uint32 Index, UMaterialInterface* InMaterial,
                             const FString& InSubMaterialName);

        const TArray<FMaterialSlot>& GetMaterialSlots() const { return MaterialSlots; }

      private:
        std::shared_ptr<FStaticMeshResource> RenderResource;
        TArray<FMaterialSlot>                MaterialSlots;
    };
} // namespace Engine::Asset