#pragma once

#include "Asset/MaterialInterface.h"

namespace Engine::Asset
{
    /**
     * @brief 빛과 표면의 상호작용을 결정하는 물리 기반 머티리얼 에셋입니다.
     */
    class ENGINE_API UMaterial : public UMaterialInterface
    {
      public:
        DECLARE_RTTI(UMaterial, UMaterialInterface)

        UMaterial();
        virtual ~UMaterial() override;

        virtual void Serialize(class FArchive& Ar) override;

      public:
        /** PBR (Physically Based Rendering) properties */
        FColor BaseColor = FColor::White();
        float  Metallic = 0.0f;
        float  Roughness = 0.5f;
        float  Emissive = 0.0f;
    };
} // namespace Engine::Asset
