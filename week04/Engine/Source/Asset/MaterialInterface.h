#pragma once

#include "Asset/Asset.h"

/**
 * @brief 메시(Mesh)에 적용될 수 있는 모든 시각적 질감의 추상 기반 클래스입니다.
 */
class ENGINE_API UMaterialInterface : public UAsset
{
  public:
    DECLARE_RTTI(UMaterialInterface, UAsset)

    UMaterialInterface();
    virtual ~UMaterialInterface() override;

    virtual void Serialize(class FArchive& Ar);
};
