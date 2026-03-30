#pragma once

#include "Actor.h"

class UStaticMeshComponent;

class ENGINE_API ACubeActor : public AActor
{
public:
	DECLARE_RTTI(ACubeActor, AActor)

	void PostSpawnInitialize() override;


private:
	UStaticMeshComponent*CubeMeshComponent = nullptr;
};