#pragma once

#include "Core/CoreMinimal.h"
#include "Particle/ParticleRandom.h"
#include "Particle/ParticleTypes.h"

struct FParticleLODLevelRuntimeCache;
class IParticleEmitterInstanceOwner;
class UParticleModule;
class UParticleEmitter;
class UParticleLODLevel;

class FParticleEmitterInstance
{
public:
	FParticleEmitterInstance(IParticleEmitterInstanceOwner& InOwner)
		: Owner(InOwner)
	{
	}

	virtual ~FParticleEmitterInstance();

	IParticleEmitterInstanceOwner& GetOwner() { return Owner; }
	const IParticleEmitterInstanceOwner& GetOwner() const { return Owner; }

	UParticleEmitter* SpriteTemplate = nullptr;

	int32 CurrentLODLevelIndex = 0;
	UParticleLODLevel* CurrentLODLevel = nullptr;
	const FParticleLODLevelRuntimeCache* CurrentRuntimeCache = nullptr;

	TArray<uint8> ParticleMemoryBlock;
	TArray<uint8> InstanceMemoryBlock;
	FParticleDataContainer DataContainer;

	uint8* ParticleData = nullptr;
	uint16* ParticleIndices = nullptr;
	uint8* InstanceData = nullptr;

	int32 ParticleSize = 0;
	int32 ParticleStride = 0;
	int32 PayloadOffset = 0;
	int32 InstancePayloadSize = 0;

	int32 ActiveParticles = 0;
	int32 MaxActiveParticles = 0;
	uint32 ParticleCounter = 0;
	float SpawnFraction = 0.0f;
	float EmitterTime = 0.0f;
	float SecondsSinceCreation = 0.0f;
	FParticleRandomStream RandomStream;

	virtual bool Init(UParticleEmitter* InTemplate, int32 InLODLevelIndex);
	virtual void Reset();
	virtual void Release();

	int32 GetActiveParticleCount() const { return ActiveParticles; }
	FBaseParticle& GetParticleByActiveIndex(int32 ActiveIndex);
	const FBaseParticle& GetParticleByActiveIndex(int32 ActiveIndex) const;
	FBaseParticle& GetParticleByPhysicalIndex(int32 PhysicalIndex);
	const FBaseParticle& GetParticleByPhysicalIndex(int32 PhysicalIndex) const;
	uint8* GetParticlePayloadByOffset(FBaseParticle& Particle, int32 Offset);
	const uint8* GetParticlePayloadByOffset(const FBaseParticle& Particle, int32 Offset) const;
	uint8* GetParticlePayload(FBaseParticle& Particle, UParticleModule* Module);
	const uint8* GetParticlePayload(const FBaseParticle& Particle, UParticleModule* Module) const;
	uint8* GetModuleInstanceData(UParticleModule* Module);
	const uint8* GetModuleInstanceData(UParticleModule* Module) const;

	bool UsesLocalSpace() const;
	FVector TransformLocationToSimulationSpace(const FVector& WorldLocation) const;
	FVector TransformVelocityToSimulationSpace(const FVector& WorldVelocity) const;
	FVector GetParticleLocationForRender(const FBaseParticle& Particle) const;
	void CalculateLocalBounds(FVector& OutMin, FVector& OutMax) const;
	void CalculateWorldBounds(FVector& OutMin, FVector& OutMax) const;

	virtual void Tick(float DeltaTime);

private:
	bool AllocateParticleData(int32 InMaxActiveParticles, int32 InParticleStride, int32 InInstancePayloadSize);

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
