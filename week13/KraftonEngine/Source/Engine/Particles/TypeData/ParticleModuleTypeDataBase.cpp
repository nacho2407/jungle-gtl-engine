#include "ParticleModuleTypeDataBase.h"

FParticleEmitterInstance* UParticleModuleTypeDataBase::CreateInstance(UParticleEmitter* InEmitter, UParticleSystemComponent& InComponent)
{
	return nullptr;
}

void UParticleModuleTypeDataBase::CacheModuleInfo(UParticleEmitter* Emitter)
{
}

void UParticleModuleTypeDataBase::Spawn(const FSpawnContext& Context)
{
}
