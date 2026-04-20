#pragma once

#include "Light.h"

class UPointLightComponent;

class APointLightActor : public ALight
{
public:
    DECLARE_CLASS(APointLightActor, ALight)
    APointLightActor() = default;

    UPointLightComponent* GetPointLightComponent() const;

    void InitDefaultComponents() override;

    float GetAttenuationRadius() const;
    float GetLightFalloffExponent() const;
};
