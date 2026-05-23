#pragma once
#include "Particles/ParticleModule.h"

class UParticleEmitter;
class IParticleEmitterInstanceOwner;
struct FParticleEmitterInstance;

#include "Source/Engine/Particles/TypeData/ParticleModuleTypeDataBase.generated.h"

UCLASS()
class UParticleModuleTypeDataBase : public UParticleModule
{
public:
	GENERATED_BODY()

	virtual FParticleEmitterInstance* CreateInstance(
		UParticleEmitter* InEmitter,
		IParticleEmitterInstanceOwner& InOwner);

	virtual EModuleType	GetModuleType() const override { return EPMT_TypeData; }

	virtual bool		IsAMeshEmitter() const { return false; }
};