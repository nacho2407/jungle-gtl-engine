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

    FLightSceneProxy* CreateLightSceneProxy() override;

	float GetAttenuationRadius() const { return AttenuationRadius; }
    float GetLightFalloffExponent() const { return LightFalloffExponent; }

protected:
	float AttenuationRadius = 1000.0f; // 1 Unit을 1cm로 계산합니다.
    float LightFalloffExponent = 8.0f;
	bool bUseInverseSquaredFalloff = true; // 감쇠 반경 내에서 빛의 세기가 거리 제곱에 반비례하도록 할지 여부 (true면 UE5의 기본 감쇠 방식, false면 단순 선형 감쇠)
};
