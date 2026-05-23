#pragma once

#include "Particles/Runtime/ParticleRuntimeTypes.h"

class UParticleEmitter;
class UParticleLODLevel;
class UParticleSystemComponent;
class UParticleModuleRequired;
class UParticleModuleSpawn;

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
	UParticleModuleRequired* GetRequiredModule() const;

	const FParticleDataContainer& GetParticleData() const { return ParticleDataContainer; }
	FParticleDataContainer& GetMutableParticleData() { return ParticleDataContainer; }
	const FParticleDataContainer& GetParticleDataContainer() const { return ParticleDataContainer; }
	FParticleDataContainer& GetMutableParticleDataContainer() { return ParticleDataContainer; }

	int32 GetActiveParticleCount() const { return ActiveParticles; }
	int32 GetMaxParticleCount() const { return MaxActiveParticles; }
	float GetEmitterTime() const { return EmitterTime; }

	template<typename T>
	T* GetParticlePayload(int32 ParticleIndex)
	{
		return GetParticlePayload<T>(ParticleIndex, PayloadOffset);
	}

	template<typename T>
	T* GetParticlePayload(int32 ParticleIndex, int32 Offset)
	{
		if (!ParticleData || ParticleIndex < 0 || ParticleIndex >= ActiveParticles)
		{
			return nullptr;
		}
		if (Offset < 0 || Offset + static_cast<int32>(sizeof(T)) > ParticleStride)
		{
			return nullptr;
		}
		const int32 ParticleSlot = ParticleIndices[ParticleIndex];
		return reinterpret_cast<T*>(ParticleData + ParticleSlot * ParticleStride + Offset);
	}

	template<typename T>
	const T* GetParticlePayload(int32 ParticleIndex) const
	{
		return GetParticlePayload<T>(ParticleIndex, PayloadOffset);
	}

	template<typename T>
	const T* GetParticlePayload(int32 ParticleIndex, int32 Offset) const
	{
		if (!ParticleData || ParticleIndex < 0 || ParticleIndex >= ActiveParticles)
		{
			return nullptr;
		}
		if (Offset < 0 || Offset + static_cast<int32>(sizeof(T)) > ParticleStride)
		{
			return nullptr;
		}
		const int32 ParticleSlot = ParticleIndices[ParticleIndex];
		return reinterpret_cast<const T*>(ParticleData + ParticleSlot * ParticleStride + Offset);
	}

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
	FBaseParticle& GetParticleBySlot(int32 ParticleSlot);
	const FBaseParticle& GetParticleBySlot(int32 ParticleSlot) const;

	UParticleModuleSpawn* GetSpawnModule() const;

	void RunSpawnModules(FBaseParticle& Particle, float SpawnTime);
	void RunUpdateModules(float DeltaTime);

	int32 MemBlockSize = 0;
	int32 ParticleDataNumBytes = 0;
	int32 ParticleIndicesNumShorts = 0;
	FParticleDataContainer ParticleDataContainer;

	float SpawnRate = 10.0f;
	float DefaultLifetime = 1.0f;
	FVector DefaultVelocity = FVector(0.0f, 0.0f, 5.0f);
	FVector DefaultSize = FVector(1.0f, 1.0f, 1.0f);
	FVector4 DefaultColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	float EmitterTime = 0.0f;
	bool bActive = true;
};
