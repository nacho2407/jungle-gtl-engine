#pragma once
#include "LightComponent.h"

UCLASS()
class UPointLightComponent : public ULightComponent
{
public:
	DECLARE_CLASS(UPointLightComponent, ULightComponent)
	virtual void PostDuplicate(UObject* Original) override;
protected:
	virtual FMatrix ComputePerspectiveShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
		const TArray<FBoundingBox>* VisibleObjectsBounds) const override;

	//virtual void PrintShadowMapDebugInfo(TArray<FPropertyDescriptor>& OutProps) const override;

public:
	UPROPERTY(DisplayName = "Attenuation Radius", Speed = 0.1f)
	float AttenuationRadius		= 10.f;

	UPROPERTY(DisplayName = "Light Falloff", Speed = 0.1f)
	float LightFalloffExponent	= 1.f;
};
