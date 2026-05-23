#pragma once

#include "Particle/ParticleTypes.h"

class UWorld;

/**
 * IParticleEmitterInstanceOwner
 * - EmitterInstance가 자신을 보유한 Component 정보에 접근하기 위한 Interface
 * - 필요한 API가 있다면 InstanceOwner에 추가하고 cpp에 구현
 */
class IParticleEmitterInstanceOwner
{
public:
	virtual ~IParticleEmitterInstanceOwner() = default;

	virtual UWorld* GetWorld() const = 0;
	virtual FVector GetWorldLocation() const = 0;
	virtual FMatrix GetComponentToWorld() const = 0;

	virtual void AddSpawnEvent(const FParticleEventSpawnData& Event) = 0;
	virtual void AddDeathEvent(const FParticleEventDeathData& Event) = 0;
	virtual void AddCollisionEvent(const FParticleEventCollideData& Event) = 0;
	virtual void AddBurstEvent(const FParticleEventBurstData& Event) = 0;
};
