#pragma once

#include "Asset/MaterialInterface.h"

namespace Engine::Asset
{
    /**
     * @brief 빛과 표면의 상호작용을 결정하는 물리 기반 머티리얼 에셋입니다.
     * 개별 MTL 머티리얼 데이터를 소유하거나 참조합니다.
     */
    class ENGINE_API UMaterial : public UMaterialInterface
    {
      public:
        DECLARE_RTTI(UMaterial, UMaterialInterface)

        UMaterial();
        virtual ~UMaterial() override;

        /** 특정 머티리얼 데이터 이름을 설정합니다. */
        void SetMaterialName(const FString& InName) { MaterialName = InName; }
        const FString& GetMaterialName() const { return MaterialName; }

        /** 실제 머티리얼 데이터(Kd, Ka 등)를 반환합니다. */
        const FMaterialData* GetMaterialData() const;

        virtual void Serialize(class FArchive& Ar) override;

      public:
        /** 에디터 편집용 추가 속성 (PBR 모방) */
        FColor BaseColor = FColor::White();
        float  Metallic = 0.0f;
        float  Roughness = 0.5f;
        float  Emissive = 0.0f;

      private:
        FString MaterialName; // MTL 파일 내의 머티리얼 이름
    };
} // namespace Engine::Asset
