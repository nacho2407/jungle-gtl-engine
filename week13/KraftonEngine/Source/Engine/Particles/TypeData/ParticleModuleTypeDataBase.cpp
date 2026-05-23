#include "ParticleModuleTypeDataBase.h"

FParticleEmitterInstance* UParticleModuleTypeDataBase::CreateInstance(UParticleEmitter* InEmitter, IParticleEmitterInstanceOwner& InOwner)
{
	return nullptr;
}

void UParticleModuleTypeDataBase::Spawn(const FSpawnContext& Context)
{
}
