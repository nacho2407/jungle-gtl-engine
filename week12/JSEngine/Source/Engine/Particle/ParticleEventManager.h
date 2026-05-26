#pragma once

#include "GameFramework/AActor.h"
#include "Particle/ParticleTypes.h"

class UParticleSystemComponent;

UCLASS()
class AParticleEventManager : public AActor
{
public:
	GENERATED_BODY(AParticleEventManager, AActor)

	void HandleParticleSpawnEvents(UParticleSystemComponent* Component, const TArray<FParticleEventSpawnData>& InSpawnEvents);
	void HandleParticleDeathEvents(UParticleSystemComponent* Component, const TArray<FParticleEventDeathData>& InDeathEvents);
	void HandleParticleCollisionEvents(UParticleSystemComponent* Component, const TArray<FParticleEventCollideData>& InCollisionEvents);
	void HandleParticleBurstEvents(UParticleSystemComponent* Component, const TArray<FParticleEventBurstData>& InBurstEvents);
};
