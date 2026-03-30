#pragma once
#pragma once

#include "Actor.h"

class USubUVComponent;

class ENGINE_API ASubUVActor : public AActor
{
public:
	DECLARE_RTTI(ASubUVActor, AActor)

	void PostSpawnInitialize() override;

private:
	USubUVComponent* SubUVComponent = nullptr;
};