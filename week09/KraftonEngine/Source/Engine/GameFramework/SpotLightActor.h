#pragma once

#include "GameFramework/AActor.h"

class USpotLightComponent;

class ASpotLightActor : public AActor
{
public:
	DECLARE_CLASS(ASpotLightActor, AActor)
	ASpotLightActor();

	void InitDefaultComponents();

private:
    USpotLightComponent* SpotLightComponent = nullptr;
};
