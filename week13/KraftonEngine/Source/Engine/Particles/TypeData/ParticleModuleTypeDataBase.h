#pragma once
#include "Particles/ParticleModule.h"

class UParticleEmitter;
class UParticleSystemComponent;
struct FParticleEmitterInstance;

#include "Source/Engine/Particles/TypeData/ParticleModuleTypeDataBase.generated.h"

UCLASS()
class UParticleModuleTypeDataBase : public UParticleModule
{
public:
	GENERATED_BODY()

	virtual FParticleEmitterInstance* CreateInstance(
		UParticleEmitter* InEmitter,
		UParticleSystemComponent& InComponent);

	virtual void CacheModuleInfo(UParticleEmitter* Emitter);

	virtual EModuleType	GetModuleType() const override { return EPMT_TypeData; }

	virtual bool		IsAMeshEmitter() const { return false; }

	virtual void Spawn(const FSpawnContext& Context) override;
};
