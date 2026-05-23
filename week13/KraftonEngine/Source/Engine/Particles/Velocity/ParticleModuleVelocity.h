#pragma once
#include "ParticleModuleVelocityBase.h"

class UParticleModuleVelocity : public UParticleModuleVelocityBase
{
public:
	// FRawDistributionVector 는 나중에 추가
	FVector MinVelocity;
	FVector MaxVelocity;
	/*
	Spawn
		Particle->Velocity = RandomVector(MinVelocity, MaxVelocity);
	*/
	virtual void Spawn(const FSpawnContext& Context) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
};