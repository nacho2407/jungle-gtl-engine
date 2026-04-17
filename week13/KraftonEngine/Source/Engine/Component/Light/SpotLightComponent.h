#pragma once
#include "Component/Light/PointLightComponent.h"

class USpotLightComponent : public UPointLightComponent
{
public:
	DECLARE_CLASS(USpotLightComponent, UPointLightComponent)

protected:
	float InnerConeAngle;	// Inner Cone Angle in degrees
	float OuterConeAngle;	// Outer Cone Angle in degrees
};