#pragma once

#include "GameFramework/AActor.h"

class UDirectionalLightComponent;

class ADirectionalLightActor : public AActor
{
public:
	DECLARE_CLASS(ADirectionalLightActor, AActor)
	ADirectionalLightActor();

	void InitDefaultComponents();

private:
    UDirectionalLightComponent* DirectionalLightComponent = nullptr;
};
