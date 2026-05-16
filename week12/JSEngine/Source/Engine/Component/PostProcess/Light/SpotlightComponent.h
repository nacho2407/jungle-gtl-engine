#pragma once
#include "PointLightComponent.h"

UCLASS()
class USpotlightComponent : public UPointLightComponent
{
public:
    DECLARE_CLASS(USpotlightComponent, UPointLightComponent)

    void PostDuplicate(UObject* Origiunal) override;
protected:
	FMatrix ComputeCascadeShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
		float SplitNearT, float SplitFarT) const override;
	FMatrix ComputePerspectiveShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
		const TArray<FBoundingBox>* VisibleObjectsBounds) const override;

public:
    UPROPERTY(DisplayName = "Inner Cone Angle", Speed = 0.1f)
    float InnerConeAngle = 10.f;

    UPROPERTY(DisplayName = "Outer Cone Angle", Speed = 0.1f)
    float OuterConeAngle = 15.f;
};
