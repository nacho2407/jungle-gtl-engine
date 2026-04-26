#pragma once
#include "PointLightComponent.h"

class USpotlightComponent : public UPointLightComponent
{
public:
    DECLARE_CLASS(USpotlightComponent, UPointLightComponent)

    void PostDuplicate(UObject* Origiunal) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	void Serialize(FArchive& Ar) override;

protected:
	FMatrix ComputePerspectiveShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
		const TArray<FBoundingBox>* VisibleObjectsBounds) const override;

public:
    float InnerConeAngle = 10.f;
    float OuterConeAngle = 15.f;
};