#pragma once

#include "Component/SceneComponent.h"
#include "Object/ObjectFactory.h"
#include "Core/EngineTypes.h"
#include "Math/Vector.h"

/** 
 * ULightComponentBase: 빛이라는 개념이 가져야 할 가장 핵심적인 데이터만 정의합니다.
 * ULightComponent: 실제 렌더링 스레드와 통신하고 화면에 그리기 위한 로직을 담당합니다.
 * 따라서 ULightComponentBase에서는 렌더링과 직접 관련 없는 조명의 속성만 정의합니다.
 */
class ULightComponentBase : public USceneComponent
{
public:
    DECLARE_CLASS(ULightComponentBase, USceneComponent)

    ULightComponentBase();

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    float GetIntensity() const { return Intensity; }
    const FVector4& GetLightColor() const { return LightColor; }
    bool AffectsWorld() const { return bAffectsWorld; }
    bool DoesCastShadows() const { return bCastShadows; }

protected:
    float Intensity = 10.0f;
    FVector4 LightColor = { 1, 1, 1, 1 }; // linear RGBA (0~1)
    bool bAffectsWorld = true; // 조명의 영향 여부를 켜고 끕니다.
    bool bCastShadows = true;  // Shadow 구현 주차에 사용: 조명이 그림자를 드리울지 여부를 켜고 끕니다.
};
