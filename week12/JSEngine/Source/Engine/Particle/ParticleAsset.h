#pragma once

#include "Object/Object.h"
#include "Particle/ParticleModules.h"

UCLASS()
class UParticleLODLevel : public UObject
{
public:
	GENERATED_BODY(UParticleLODLevel, UObject)
	~UParticleLODLevel() override;

	int32 Level = 0;
	bool bEnabled = true;

	UParticleModuleRequired* RequiredModule = nullptr;
	TArray<UParticleModule*> Modules;
	UParticleModuleTypeDataBase* TypeDataModule = nullptr;
};

UCLASS()
class UParticleEmitter : public UObject
{
public:
	GENERATED_BODY(UParticleEmitter, UObject)
	~UParticleEmitter() override;

	TArray<UParticleLODLevel*> LODLevels;
	TArray<int32> ParticleSize;

	void CacheEmitterModuleInfo();
	TArray<int32> CalculateTotalPayloadSize() const;
};

UCLASS()
class UParticleSystem : public UObject
{
public:
	GENERATED_BODY(UParticleSystem, UObject)
	~UParticleSystem() override;

	TArray<UParticleEmitter*> Emitters;
};
