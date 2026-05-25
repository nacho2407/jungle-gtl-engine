#include "Particle/ParticleEventManager.h"

#include "Particle/ParticleSystemComponent.h"

void AParticleEventManager::HandleParticleSpawnEvents(UParticleSystemComponent* Component, const TArray<FParticleEventSpawnData>& InSpawnEvents)
{
	if (Component == nullptr)
	{
		return;
	}

	for (const FParticleEventSpawnData& Event : InSpawnEvents)
	{
		Component->OnParticleSpawn.Broadcast(Component, Event);
	}
}

void AParticleEventManager::HandleParticleDeathEvents(UParticleSystemComponent* Component, const TArray<FParticleEventDeathData>& InDeathEvents)
{
	if (Component == nullptr)
	{
		return;
	}

	for (const FParticleEventDeathData& Event : InDeathEvents)
	{
		Component->OnParticleDeath.Broadcast(Component, Event);
	}
}

void AParticleEventManager::HandleParticleCollisionEvents(UParticleSystemComponent* Component, const TArray<FParticleEventCollideData>& InCollisionEvents)
{
	if (Component == nullptr)
	{
		return;
	}

	for (const FParticleEventCollideData& Event : InCollisionEvents)
	{
		Component->OnParticleCollide.Broadcast(Component, Event);
	}
}

void AParticleEventManager::HandleParticleBurstEvents(UParticleSystemComponent* Component, const TArray<FParticleEventBurstData>& InBurstEvents)
{
	if (Component == nullptr)
	{
		return;
	}

	for (const FParticleEventBurstData& Event : InBurstEvents)
	{
		Component->OnParticleBurst.Broadcast(Component, Event);
	}
}
