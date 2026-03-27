#pragma once

#include "Actor.h"

class USubUVComponent;

class ENGINE_API ASubUVActor : public AActor
{
public:
	DECLARE_RTTI(ASubUVActor, AActor)

	void PostSpawnInitialize() override;

	USubUVComponent* GetSubUVComponent() const { return SubUVComponent; }

private:
	USubUVComponent* SubUVComponent = nullptr;
};