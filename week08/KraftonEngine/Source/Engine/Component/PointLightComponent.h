#pragma once

#include "LightComponent.h"

class UPointLightComponent : public ULightComponent
{
public:
    DECLARE_CLASS(UPointLightComponent, ULightComponent)
    UPointLightComponent() = default;

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

	float GetAttenuationRadius() const { return AttenuationRadius; }
    float GetLightFalloffExponent() const { return LightFalloffExponent; }

protected:
	float AttenuationRadius = 1000.0f; // 1 Unit을 1cm로 계산합니다.
    float LightFalloffExponent = 8.0f;

	// ─── UE5에 있는 프로퍼티 속성들 (에디터 창 노출, 직렬화 X) ───
	// float SoftSourceRadius = 0.0f; // 광원(Light Source)이 차지하는 물리적인 구형 부피의 반경
    // float SourceLength = 0.0f;     // 광원을 점(Point)이 아닌 선 또는 캡슐 모양으로 늘리는 값
	// bool bUseInverseSquaredFalloff = true; // 감쇠 반경 내에서 빛의 세기가 거리 제곱에 반비례하도록 할지 여부 (true면 UE5의 기본 감쇠 방식, false면 단순 선형 감쇠)
};
