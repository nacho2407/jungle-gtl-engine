#pragma once
#include "LightComponent.h"

UCLASS(SpawnableComponent, DisplayName = "DirectionalLight Component", Category = "Light")
class UDirectionalLightComponent : public ULightComponent
{
public:
	GENERATED_BODY(UDirectionalLightComponent, ULightComponent)
protected:
	FMatrix ComputePerspectiveShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
		const TArray<FBoundingBox>* VisibleObjectsBounds) const override;

public:
	UPROPERTY(DisplayName = "MaxDistance", Min = 0.0f, Max = 1000.0f, Speed = 10.0f)
	float CSMMaxDistance = { 300.f };

	UPROPERTY(DisplayName = "Lambda", Min = 0.0f, Max = 1.0f, Speed = 0.01f)
	float CSMPractialLambda = { 0.25f };

};
