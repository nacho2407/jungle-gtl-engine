#pragma once

#include "Actor.h"

class UPrimitiveComponent;

class ENGINE_API AAttachTestActor : public AActor
{
public:
	DECLARE_RTTI(AAttachTestActor, AActor)

	void PostSpawnInitialize() override;

	UPrimitiveComponent* GetSphereComponent() const { return SphereComponent; }
	UPrimitiveComponent* GetCubeComponent() const { return CubeComponent; }

private:
	UPrimitiveComponent* SphereComponent = nullptr;
	UPrimitiveComponent* CubeComponent = nullptr;
};
