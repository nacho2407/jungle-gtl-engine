#pragma once

#include "Particles/Runtime/ParticleRuntimeTypes.h"

class UParticleEmitter;
class UParticleLODLevel;
class UParticleSystemComponent;

struct FParticleEmitterInstance
{
	virtual ~FParticleEmitterInstance() = default;

	virtual void Init(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent);
	virtual void Tick(float DeltaTime);
	virtual void Reset();
	virtual FDynamicEmitterDataBase* BuildRenderData() { return nullptr; }

	bool IsActive() const { return bActive; }
	void SetActive(bool bInActive) { bActive = bInActive; }

	UParticleEmitter* GetTemplate() const { return SpriteTemplate; }
	UParticleSystemComponent* GetComponent() const { return Component; }
	UParticleLODLevel* GetCurrentLODLevel() const { return CurrentLODLevel; }

	int32 GetActiveParticleCount() const { return ActiveParticles; }
	float GetEmitterTime() const { return EmitterTime; }

protected:
	UParticleEmitter* SpriteTemplate = nullptr;
	UParticleSystemComponent* Component = nullptr;

	int32 CurrentLODLevelIndex = 0;
	UParticleLODLevel* CurrentLODLevel = nullptr;

	int32 ActiveParticles = 0;
	uint32 ParticleCounter = 0;
	int32 MaxActiveParticles = 0;
	float EmitterTime = 0.0f;
	bool bActive = true;
};
