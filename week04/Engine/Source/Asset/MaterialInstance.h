#pragma once
#include "MaterialInterface.h"

struct FMaterialData;

namespace Engine::Asset
{
    /**
     * @brief 런타임에 동적으로 재질 속성을 변경하기 위한 인스턴스 클래스입니다.
     * 원본 에셋(UMaterialAsset)을 부모로 참조하며, 변경된 데이터만 따로 저장(Override)합니다.
     */
    class ENGINE_API UMaterialInstance : public UMaterialInterface
    {
        DECLARE_RTTI(UMaterialInstance, UMaterialInterface)
      public:
        UMaterialInstance() = default;
        ~UMaterialInstance() = default;

        // --- 부모 계층 설정 ---
        void                SetParent(UMaterialInterface* InParent);
        UMaterialInterface* GetParent() const { return Parent; }

        // --- 다형성 인터페이스 구현 ---
        virtual const FMaterialData* GetMaterialData(const FString& SubMaterialName) const override;

        // --- 오버라이드 제어 ---
        // 특정 서브 머티리얼(usemtl)의 데이터를 통째로 덮어씌웁니다.
        void SetMaterialDataOverride(const FString& SubMaterialName, const FMaterialData& NewData);

        // 특정 서브 머티리얼의 오버라이드를 취소하고 다시 부모의 값을 따르게 합니다.
        void ClearMaterialDataOverride(const FString& SubMaterialName);
        void ClearAllOverrides();

      private:
        UMaterialInterface* Parent = nullptr;

        // 서브 머티리얼 이름(usemtl)을 키로 하여 변경된 데이터만 저장하는 맵
        TMap<FString, FMaterialData> OverriddenData;
    };
} // namespace Engine::Asset