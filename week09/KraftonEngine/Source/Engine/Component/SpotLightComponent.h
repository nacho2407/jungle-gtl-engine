#pragma once

#include "PointLightComponent.h"

class USpotLightComponent : public UPointLightComponent
{
public:
    DECLARE_CLASS(USpotLightComponent, UPointLightComponent)
    USpotLightComponent() = default;

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	float GetInnerConeAngle() const { return InnerConeAngle; }
    float GetOuterConeAngle() const { return OuterConeAngle; }

	float GetHalfConeAngle() const;    // OuterConeAngle의 절반을 라디안으로 반환
    float GetCosHalfConeAngle() const; // OuterConeAngle의 절반의 코사인 값을 반환

private:
	// ─── UE5 Default Value ───
    float InnerConeAngle = 0.0f;  // SpotLight 중심 빛이 최대 강도를 가지는 원뿔의 각도
    float OuterConeAngle = 44.0f; // SpotLight의 빛이 완전히 사라지는 원뿔의 각도
};
