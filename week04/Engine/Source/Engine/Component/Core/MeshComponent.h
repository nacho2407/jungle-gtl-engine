#pragma once

#include "Engine/Component/Core/PrimitiveComponent.h"

namespace Engine::Asset
{
    class UMaterial;
}

namespace Engine::Component
{
    /**
     * @brief 모든 메시 계열 컴포넌트의 기반 클래스입니다.
     * 머티리얼 슬롯 관리 등 공통 기능을 담당합니다.
     */
    class ENGINE_API UMeshComponent : public UPrimitiveComponent
    {
      public:
        DECLARE_RTTI(UMeshComponent, UPrimitiveComponent)

        UMeshComponent();
        virtual ~UMeshComponent() override;

        /** 머티리얼 슬롯 초기화 (에셋의 섹션 수에 따라 호출) */
        virtual void InitializeMaterialSlots(uint32 NumSections);

        /** 머티리얼 설정 및 반환 */
        virtual void              SetMaterial(uint32 Index, Asset::UMaterial* InMaterial);
        virtual Asset::UMaterial* GetMaterial(uint32 Index) const;
        virtual uint32            GetNumMaterials() const;

        virtual void Serialize(bool bIsLoading, void* JsonHandle);

      protected:
        /** 실제 렌더링 시 사용할 머티리얼 리스트 (다중 섹션 대응) */
        TArray<Asset::UMaterial*> OverrideMaterials;
    };
} // namespace Engine::Component
