#pragma once

#include "Light.h"

class AAmbientLightActor : public ALight
{
public:
    DECLARE_CLASS(AAmbientLightActor, ALight)
    AAmbientLightActor() = default;

    virtual void InitDefaultComponents() override;
};
