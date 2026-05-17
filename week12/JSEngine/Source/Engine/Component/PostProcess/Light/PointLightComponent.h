#pragma once
#include "LightComponent.h"

UCLASS(SpawnableComponent, DisplayName = "PointLight Component", Category = "Light")
class UPointLightComponent : public ULightComponent
{
public:
	GENERATED_BODY(UPointLightComponent, ULightComponent)
	virtual void PostDuplicate(UObject* Original) override;
protected:
	virtual FMatrix ComputePerspectiveShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
		const TArray<FBoundingBox>* VisibleObjectsBounds) const override;

public:
	UPROPERTY(DisplayName = "Attenuation Radius", Speed = 0.1f)
	float AttenuationRadius		= 10.f;

	UPROPERTY(DisplayName = "Light Falloff", Speed = 0.1f)
	float LightFalloffExponent	= 1.f;
};
