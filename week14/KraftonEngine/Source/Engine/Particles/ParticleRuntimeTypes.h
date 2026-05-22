#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"

class UParticleEmitter;
class UParticleLODLevel;
class UParticleSystemComponent;

struct FBaseParticle
{
	FVector Position;
	FVector OldPosition;
	FVector Velocity;

	FVector Size;
	float Rotation = 0.0f;
	float RotationRate = 0.0f;

	FVector4 Color = { 1, 1, 1, 1 };

	float RelativeTime = 0.0f;
	float OneOverMaxLifetime = 1.0f;
	float Lifetime = 1.0f;
	float Age = 0.0f;

	uint32 FrameIndex = 0;
	bool bAlive = false;
};

//나 데이터베이스 아임다 데이터 Base 임다
struct FDynamicEmitterDataBase
{
	virtual ~FDynamicEmitterDataBase() = default;
};

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
