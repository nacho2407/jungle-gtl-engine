#pragma once
#include "Component/Light/LightComponent.h"

class UPointLightComponent : public ULightComponent
{
public:
	DECLARE_CLASS(UPointLightComponent, ULightComponent)

protected:
	float AttenuationRadius;
	float LightFalloffExponent;
};