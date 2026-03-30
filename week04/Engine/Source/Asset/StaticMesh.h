#pragma once
#include "StreamableRenderAsset.h"
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

    /**
     * @brief 정적 메시(Static Mesh) 리소스를 관리하는 에셋 클래스입니다.
     * 언리얼 엔진의 UStaticMesh 구조를 모방하며, 실제 데이터는 FStaticMeshResource가 소유합니다.
     */
    class ENGINE_API UStaticMesh : public UStreamableRenderAsset
    {
        DECLARE_RTTI(UStaticMesh, UStreamableRenderAsset)
      public:
        UStaticMesh() = default;
        virtual ~UStaticMesh() override = default;

        /** 에셋 로더로부터 리소스를 주입받아 초기화합니다. */
        void Initialize(const FSourceRecord& InSource, std::shared_ptr<FStaticMeshResource> InResource);

        /** 렌더링 리소스 접근 */
        const FStaticMeshResource* GetRenderResource() const { return RenderResource.get(); }
        FStaticMeshResource*       GetRenderResource() { return RenderResource.get(); }

        /** 머티리얼 슬롯 관리 */
        void InitializeMaterialSlots(uint32 NumSlots);
        const FMaterialSlot* GetMaterialSlot(uint32 Index) const;
        FMaterialSlot*       GetMaterialSlot(uint32 Index);
        UMaterialInterface*  GetMaterial(uint32 Index) const;
        const FString&       GetSubMaterialName(uint32 Index) const;
        void SetMaterialSlot(uint32 Index, UMaterialInterface* InMaterial, const FString& InSubMaterialName);

        const TArray<FMaterialSlot>& GetMaterialSlots() const { return MaterialSlots; }
        uint32 GetNumSections() const { return static_cast<uint32>(MaterialSlots.size()); }

      public:
        bool bIsBaked = false;

      private:
        std::shared_ptr<FStaticMeshResource> RenderResource;
        TArray<FMaterialSlot>                MaterialSlots;
    };
} // namespace Engine::Asset
