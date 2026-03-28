#pragma once

#include "Asset/MaterialInterface.h"

namespace Engine::Asset
{
    /**
     * @brief 원본 머티리얼의 수식을 공유하되, 파라미터 값만 재정의하는 클래스입니다.
     */
    class ENGINE_API UMaterialInstance : public UMaterialInterface
    {
      public:
        DECLARE_RTTI(UMaterialInstance, UMaterialInterface)

        UMaterialInstance();
        virtual ~UMaterialInstance() override;

        /** 부모 머티리얼 설정 */
        void                SetParent(UMaterialInterface* InParent) { Parent = InParent; }
        UMaterialInterface* GetParent() const { return Parent; }

        /** 재정의할 속성들 */
        FColor BaseColorOverride = FColor::White();
        bool   bOverrideBaseColor = false;

        float MetallicOverride = 0.0f;
        bool  bOverrideMetallic = false;

        float RoughnessOverride = 0.5f;
        bool  bOverrideRoughness = false;

      private:
        /** 참조하는 부모 머티리얼 인터페이스 */
        UMaterialInterface* Parent = nullptr;
    };
} // namespace Engine::Asset
