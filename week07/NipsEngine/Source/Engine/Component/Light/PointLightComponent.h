#pragma once

#include "LightComponent.h"

class UPointLightComponent : public ULightComponent
{
public:
	DECLARE_CLASS(UPointLightComponent, ULightComponent)

	UPointLightComponent() = default;
	virtual ~UPointLightComponent() override = default;

	virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	virtual void Serialize(FArchive& Ar) override;

	float GetAttenuationRadius() const { return AttenuationRadius; }
	void SetAttenuationRadius(float InRadius) { AttenuationRadius = InRadius; }
	float GetLightFalloffExponent() const { return LightFalloffExponent; }
	void SetLightFalloffExponent(float InExponent) { LightFalloffExponent = InExponent; }

private:
	float AttenuationRadius = 1000.0f;
	float LightFalloffExponent = 2.0f;
};
