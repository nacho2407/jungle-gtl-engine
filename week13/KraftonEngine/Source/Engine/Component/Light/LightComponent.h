#pragma once
#include "Component/Light/LightComponentBase.h"

class ULightComponent : public ULightComponentBase
{
public:
	DECLARE_CLASS(ULightComponent, ULightComponentBase)

	virtual void Serialize(FArchive& Ar) override;
	virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	float GetShadowResolutionScale() const { return ShadowResolutionScale; }
	float GetShadowBias() const { return ShadowBias; }
	float GetShadowSlopeBias() const { return ShadowSlopeBias; }
	float GetShadowSharpen() const { return ShadowSharpen; }

protected:
	float ShadowResolutionScale = 1.0f;
	float ShadowBias			= 0.005f;
	float ShadowSlopeBias		= 0.005f;
	float ShadowSharpen			= 0.67f;

	// Scale^2 = No. of samples used to compute average depth. 1 = No PCF applied
	uint8 PCFScale				= 1;
};
