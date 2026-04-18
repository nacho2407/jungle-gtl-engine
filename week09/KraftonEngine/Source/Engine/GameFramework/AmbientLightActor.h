#pragma once

#include "GameFramework/AActor.h"

class UAmbientLightComponent;

class AAmbientLightActor : public AActor
{
public:
	DECLARE_CLASS(AAmbientLightActor, AActor)
	AAmbientLightActor();

	void InitDefaultComponents();

private:
    UAmbientLightComponent* AmbientLightComponent = nullptr;
};
