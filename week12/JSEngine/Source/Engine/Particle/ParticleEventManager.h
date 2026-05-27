#pragma once

#include "GameFramework/AActor.h"
#include "Particle/ParticleTypes.h"

class UParticleSystemComponent;

UCLASS()
class AParticleEventManager : public AActor
{
public:
	GENERATED_BODY(AParticleEventManager, AActor)

	void HandleParticleCollisionEvents(UParticleSystemComponent* Component, const TArray<FParticleEventCollideData>& InCollisionEvents);
};
