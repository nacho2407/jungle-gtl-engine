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
	float GetShadowNormalBias() const { return ShadowNormalBias; } 
	float GetShadowSharpen() const { return ShadowSharpen; }

	void SetShadowBias(float V) { ShadowBias = V; }
	void SetShadowSlopeBias(float V) { ShadowSlopeBias = V; }
	void SetShadowNormalBias(float V) { ShadowNormalBias = V; }
	void SetShadowSharpen(float V) { ShadowSharpen = V; }

protected:
	float ShadowResolutionScale = 1.0f;
	float ShadowBias = -0.0001f;
	float ShadowSlopeBias = 0.0001f;
	float ShadowNormalBias = -0.0020f;
	float ShadowSharpen = 0.67f;
};
