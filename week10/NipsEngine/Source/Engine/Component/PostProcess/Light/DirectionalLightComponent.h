#pragma once
#include "LightComponent.h"

class UDirectionalLightComponent : public ULightComponent
{
public:
    DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)
    void PostDuplicate(UObject* original) override;
	virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
};