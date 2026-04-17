#pragma once
#include "Component/Light/PointLightComponent.h"

class USpotLightComponent : public UPointLightComponent
{
public:
	DECLARE_CLASS(USpotLightComponent, UPointLightComponent)
	virtual void ContributeSelectedVisuals(FScene& Scene) const override;
	virtual void PushToScene() override;
	virtual void DestroyFromScene() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

protected:
	float InnerConeAngle = 20.0f;	// Inner Cone Angle in degrees
	float OuterConeAngle = 40.0f;	// Outer Cone Angle in degrees
};
