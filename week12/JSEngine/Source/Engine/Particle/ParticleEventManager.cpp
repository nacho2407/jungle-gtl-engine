#include "Particle/ParticleEventManager.h"

#include "Particle/ParticleSystemComponent.h"

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
