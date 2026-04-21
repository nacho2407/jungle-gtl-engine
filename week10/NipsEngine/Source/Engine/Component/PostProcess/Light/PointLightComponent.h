#pragma once
#include "LightComponent.h"

class UPointLightComponent : public ULightComponent
{
public:
    DECLARE_CLASS(UPointLightComponent, ULightComponent)
    virtual void PostDuplicate(UObject* Original) override;
    virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	virtual void Serialize(FArchive& Ar) override;

public:
    float AttenuationRadius		= 10.f;
    float LightFalloffExponent	= 1.f;
};