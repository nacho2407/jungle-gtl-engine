#pragma once

#include "Core/CoreMinimal.h"

class IParticleEmitterInstanceOwner;
class UParticleEmitter;
class UParticleLODLevel;

class FParticleEmitterInstance
{
public:
	FParticleEmitterInstance(IParticleEmitterInstanceOwner& InOwner)
		: Owner(InOwner)
	{
	}

	virtual ~FParticleEmitterInstance() = default;

	IParticleEmitterInstanceOwner& GetOwner() { return Owner; }
	const IParticleEmitterInstanceOwner& GetOwner() const { return Owner; }

	UParticleEmitter* SpriteTemplate = nullptr;

	int32 CurrentLODLevelIndex = 0;
	UParticleLODLevel* CurrentLODLevel = nullptr;

	uint8* ParticleData = nullptr;
	int32 ParticleStride = 0;
	int32 ActiveParticles = 0;
	int32 MaxActiveParticles = 0;

	virtual void Tick(float DeltaTime);

private:
	IParticleEmitterInstanceOwner& Owner;
};

class FParticleMeshEmitterInstance : public FParticleEmitterInstance
{
public:
	explicit FParticleMeshEmitterInstance(IParticleEmitterInstanceOwner& InOwner)
		: FParticleEmitterInstance(InOwner)
	{
	}
};

class FParticleBeamEmitterInstance : public FParticleEmitterInstance
{
public:
	explicit FParticleBeamEmitterInstance(IParticleEmitterInstanceOwner& InOwner)
		: FParticleEmitterInstance(InOwner)
	{
	}
};

class FParticleTrailsEmitterInstance : public FParticleEmitterInstance
{
public:
	explicit FParticleTrailsEmitterInstance(IParticleEmitterInstanceOwner& InOwner)
		: FParticleEmitterInstance(InOwner)
	{
	}
};

class FParticleRibbonEmitterInstance : public FParticleTrailsEmitterInstance
{
public:
	explicit FParticleRibbonEmitterInstance(IParticleEmitterInstanceOwner& InOwner)
		: FParticleTrailsEmitterInstance(InOwner)
	{
	}
};
