#include "Particles/RotationRate/ParticleModuleMeshRotationRate.h"

#include "Particles/ParticleEmitterInstances.h"
#include "Serialization/Archive.h"

UParticleModuleMeshRotationRate::UParticleModuleMeshRotationRate()
{
	bSpawnModule = true;
	bUpdateModule = false;
}

void UParticleModuleMeshRotationRate::Spawn(const FSpawnContext& Context)
{
	FParticleMeshEmitterInstance* MeshInst = dynamic_cast<FParticleMeshEmitterInstance*>(&Context.Owner);
	if (!MeshInst || MeshInst->MeshRotationOffset <= 0)
	{
		return;
	}
	FMeshRotationPayloadData* Payload = reinterpret_cast<FMeshRotationPayloadData*>(reinterpret_cast<uint8*>(Context.ParticleBase) + MeshInst->MeshRotationOffset);
	Payload->RotationRateBase = StartRotationRate.GetValue(Context.Owner.EmitterTime, Context.GetDistributionData());
	Payload->RotationRate = Payload->RotationRateBase;
}

void UParticleModuleMeshRotationRate::Serialize(FArchive& Ar)
{
	UParticleModule::Serialize(Ar);
	StartRotationRate.Serialize(Ar);
}
