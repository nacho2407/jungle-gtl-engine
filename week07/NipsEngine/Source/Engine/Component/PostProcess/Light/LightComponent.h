#pragma once
#include "LightComponentBase.h"

class UMaterialInterface;

class ULightComponent : public ULightComponentBase {
public:
	DECLARE_CLASS(ULightComponent, ULightComponentBase)
	ULightComponent() = default;
	
	// PostDUplicate is not overridden in LightComponent.h

protected:
	~ULightComponent() = default;


public:
	UMaterialInterface* LightFunctionMaterial = nullptr;

	float DiffuseScale		= 0.f;
	float SpecularScale		= 0.f;
};