#pragma once

#include "Light.h"

class AmbientLight : public ALight
{
public:
	DECLARE_CLASS(AmbientLight, ALight)
	AmbientLight() = default;

    virtual void InitDefaultComponents() override;
}
