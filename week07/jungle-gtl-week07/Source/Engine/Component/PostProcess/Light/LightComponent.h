#pragma once
#include "LightComponentBase.h"

class UMaterialInterface;

class ULightComponent : public ULightComponentBase {
public:
	DECLARE_CLASS(ULightComponent, ULightComponentBase)
	ULightComponent() = default;
	
protected:
	~ULightComponent() = default;
};