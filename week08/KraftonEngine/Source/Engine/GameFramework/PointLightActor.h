#pragma once

#include "GameFramework/AActor.h"

class UPointLightComponent;

class APointLightActor : public AActor
{
public:
	DECLARE_CLASS(APointLightActor, AActor)
	APointLightActor();

	void InitDefaultComponents();

private:
    UPointLightComponent* PointLightComponent = nullptr;
};
