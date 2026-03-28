#pragma once

#include "Engine/Component/Core/PrimitiveComponent.h"

namespace Engine::Asset
{
    class UMaterial;
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

        /** 머티리얼 슬롯 관리 */
        virtual void              InitializeMaterialSlots(uint32 NumSections);
        virtual void              SetMaterial(uint32 Index, Asset::UMaterial* InMaterial);
        virtual Asset::UMaterial* GetMaterial(uint32 Index) const;
        virtual uint32            GetNumMaterials() const;

        virtual void DescribeProperties(FComponentPropertyBuilder& Builder) override;

        virtual void Serialize(bool bIsLoading, void* JsonHandle);

      protected:
        TArray<Asset::UMaterial*> OverrideMaterials;
    };
} // namespace Engine::Component
