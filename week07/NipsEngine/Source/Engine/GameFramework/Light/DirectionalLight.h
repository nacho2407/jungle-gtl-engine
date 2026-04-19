#pragma once

#include "Light.h"

class ADirectionalLightActor : public ALight
{
public:
    DECLARE_CLASS(ADirectionalLightActor, ALight)
    ADirectionalLightActor() = default;

	virtual void InitDefaultComponents() override;

	FVector GetLightDirection() const;
};
