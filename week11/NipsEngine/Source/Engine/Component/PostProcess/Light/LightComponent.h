#pragma once
#include "LightComponentBase.h"

class UMaterialInterface;

class ULightComponent : public ULightComponentBase {
public:
	DECLARE_CLASS(ULightComponent, ULightComponentBase)
	ULightComponent() = default;

	FMatrix GetPSMMatrix(const FMatrix& CamView, const FMatrix& CamProj) const;
	
protected:
	~ULightComponent() = default;

public:
	float ShadowResolutionScale = 1024.0f;
	float ShadowBias = 0.5f;
	float ShadowSlopeBias = 0.5f;
	float ShadowSharpen = 0.5f;
};