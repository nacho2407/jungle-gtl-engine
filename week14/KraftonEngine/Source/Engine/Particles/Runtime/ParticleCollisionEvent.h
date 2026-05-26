#pragma once

#include "Core/Types/EngineTypes.h"
#include "Object/FName.h"

class UPrimitiveComponent;

struct FParticleCollisionEventPayload
{
	FName EventName;
	float EmitterTime;
	FVector Location;
	FVector Normal;
	FVector Velocity;
	FVector Direction;
	int32 ParticleIndex;
	UPrimitiveComponent* HitComponent;
};