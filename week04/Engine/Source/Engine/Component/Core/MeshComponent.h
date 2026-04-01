#pragma once

#include "Engine/Component/Core/PrimitiveComponent.h"

namespace Engine::Asset
{
    class UMaterialInterface;
}

namespace Engine::Component
{
    /**
     * @brief 모든 메시 계열 컴포넌트의 공통 기반 클래스입니다. (UE 표준)
     */
    class ENGINE_API UMeshComponent : public UPrimitiveComponent
    {
      public:
        DECLARE_RTTI(UMeshComponent, UPrimitiveComponent)

        UMeshComponent();
        virtual ~UMeshComponent() override;

        virtual void Serialize(bool bIsLoading, void* JsonHandle) override;

        /** 머티리얼 슬롯 관리 */
        virtual void InitializeMaterialSlots(uint32 NumSections);
        virtual void SetMaterial(uint32 Index, Asset::UMaterialInterface* InMaterial);
        virtual Asset::UMaterialInterface* GetMaterial(uint32 Index) const;
        virtual uint32                     GetNumMaterials() const;

        virtual void DescribeProperties(FComponentPropertyBuilder& Builder) override;

        /** 경로 변환 유틸리티 */
        static FString WidePathToUtf8(const FWString& Path);

      protected:
        TArray<Asset::UMaterialInterface*> OverrideMaterials;
    };
} // namespace Engine::Component
