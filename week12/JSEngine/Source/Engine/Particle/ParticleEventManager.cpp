#include "Particle/ParticleEventManager.h"

#include "Particle/ParticleSystemComponent.h"

void AParticleEventManager::HandleParticleSpawnEvents(UParticleSystemComponent* Component, const TArray<FParticleEventSpawnData>& InSpawnEvents)
{
	(void)Component;
	(void)InSpawnEvents;
}

void AParticleEventManager::HandleParticleDeathEvents(UParticleSystemComponent* Component, const TArray<FParticleEventDeathData>& InDeathEvents)
{
	(void)Component;
	(void)InDeathEvents;
}

void AParticleEventManager::HandleParticleCollisionEvents(UParticleSystemComponent* Component, const TArray<FParticleEventCollideData>& InCollisionEvents)
{
	(void)Component;
	(void)InCollisionEvents;
}

void AParticleEventManager::HandleParticleBurstEvents(UParticleSystemComponent* Component, const TArray<FParticleEventBurstData>& InBurstEvents)
{
	(void)Component;
	(void)InBurstEvents;
}
