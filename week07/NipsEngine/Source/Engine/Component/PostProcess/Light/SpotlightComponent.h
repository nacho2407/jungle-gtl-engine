#pragma once
#include "PointLightComponent.h"

class USpotlightComponent : public UPointLightComponent
{
public:
    DECLARE_CLASS(USpotlightComponent, UPointLightComponent)

    void PostDuplicate(UObject* Origiunal) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

public:
    FVector Direction = FVector(0, 0, 1);
    float InnerConeAngle = 10.f;
    float OuterConeAngle = 15.f;
};