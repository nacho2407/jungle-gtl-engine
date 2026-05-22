#pragma once

#include "Particles/Runtime/ParticleRuntimeTypes.h"

class UParticleEmitter;
class UParticleLODLevel;
class UParticleSystemComponent;

struct FParticleEmitterInstance
{
	virtual ~FParticleEmitterInstance();

	virtual void Init(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent);
	virtual void Tick(float DeltaTime);
	virtual void Reset();

	bool IsActive() const { return bActive; }
	void SetActive(bool bInActive) { bActive = bInActive; }

	UParticleEmitter* GetTemplate() const { return SpriteTemplate; }
	UParticleSystemComponent* GetComponent() const { return Component; }
	UParticleLODLevel* GetCurrentLODLevel() const { return CurrentLODLevel; }

	const FParticleDataContainer& GetParticleData() const { return ParticleDataContainer; }
	FParticleDataContainer& GetMutableParticleData() { return ParticleDataContainer; }
	const FParticleDataContainer& GetParticleDataContainer() const { return ParticleDataContainer; }
	FParticleDataContainer& GetMutableParticleDataContainer() { return ParticleDataContainer; }

	int32 GetActiveParticleCount() const { return ActiveParticles; }
	int32 GetMaxParticleCount() const { return MaxActiveParticles; }
	float GetEmitterTime() const { return EmitterTime; }

	UParticleEmitter* SpriteTemplate = nullptr;
	UParticleSystemComponent* Component = nullptr;

	int32 CurrentLODLevelIndex = 0;
	UParticleLODLevel* CurrentLODLevel = nullptr;

	uint8* ParticleData = nullptr;
	uint16* ParticleIndices = nullptr;
	uint8* InstanceData = nullptr;
	int32 InstancePayloadSize = 0;
	int32 PayloadOffset = 0;
	int32 ParticleSize = 0;
	int32 ParticleStride = 0;
	int32 ActiveParticles = 0;
	uint32 ParticleCounter = 0;
	int32 MaxActiveParticles = 0;
	float SpawnFraction = 0.0f;

protected:
	virtual int32 SpawnParticles(float DeltaTime);
	virtual void InitializeParticle(FBaseParticle& Particle);
	virtual void UpdateParticles(float DeltaTime);
	virtual void KillParticle(int32 ParticleIndex);

	void AllocateParticleData(int32 InMaxActiveParticles);
	void ReleaseParticleData();
	FBaseParticle* SpawnParticle();
	FBaseParticle& GetParticle(int32 ParticleIndex);
	const FBaseParticle& GetParticle(int32 ParticleIndex) const;

	int32 MemBlockSize = 0;
	int32 ParticleDataNumBytes = 0;
	int32 ParticleIndicesNumShorts = 0;
	FParticleDataContainer ParticleDataContainer;

	float SpawnRate = 10.0f;
	float DefaultLifetime = 1.0f;
	FVector DefaultVelocity = FVector(0.0f, 0.0f, 100.0f);
	FVector DefaultSize = FVector(10.0f, 10.0f, 1.0f);
	FVector4 DefaultColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	float EmitterTime = 0.0f;
	bool bActive = true;
};
