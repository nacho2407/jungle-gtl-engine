#pragma once

#include "Core/Types/EngineTypes.h"
#include "Object/FName.h"
#include "Object/Reflection/ObjectMacros.h"

class UPrimitiveComponent;

UENUM()
enum class EParticleEventType : uint8
{
	Spawn = 0,
	Kill = 1,
	Collision = 2
};

struct FParticleCollisionEventPayload
{
	FName EventName;
	EParticleEventType EventType = EParticleEventType::Collision;
	float EmitterTime;
	FVector Location;
	FVector Normal;
	FVector Velocity;
	FVector Direction;
	int32 ParticleIndex;
	UPrimitiveComponent* HitComponent;
};
