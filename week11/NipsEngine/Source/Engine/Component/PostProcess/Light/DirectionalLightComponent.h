#pragma once
#include "LightComponent.h"

class UDirectionalLightComponent : public ULightComponent
{
public:
    DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)
    void PostDuplicate(UObject* original) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

public:
    FVector LightDirection = FVector(0.f, 0.f, -1.f);
};