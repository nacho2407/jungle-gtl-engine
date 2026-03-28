#pragma once

#include "Asset/Asset.h"

namespace Engine::Asset
{
    /**
     * @brief 메시(Mesh)에 적용될 수 있는 모든 시각적 질감의 추상 기반 클래스입니다.
     */
    class ENGINE_API UMaterialInterface : public UAsset
    {
      public:
        DECLARE_RTTI(UMaterialInterface, UAsset)

        UMaterialInterface();
        virtual ~UMaterialInterface() override;

        /** UAsset에 Serialize가 없으므로 여기서 새롭게 가상 함수로 정의합니다. */
        virtual void Serialize(class FArchive& Ar);
    };
} // namespace Engine::Asset
