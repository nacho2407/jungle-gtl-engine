#pragma once

#include "Actor.h"

class USkyComponent;

class ENGINE_API ASkySphereActor : public AActor
{
public:
	DECLARE_RTTI(ASkySphereActor, AActor)

	void PostSpawnInitialize() override;

private:
	USkyComponent* SkySphereComponent = nullptr;
};