#include "Particles/RotationRate/ParticleModuleMeshRotationRate.h"

#include "Particles/ParticleEmitterInstances.h"
#include "Serialization/Archive.h"

UParticleModuleMeshRotationRate::UParticleModuleMeshRotationRate()
{
	bSpawnModule = true;
	bUpdateModule = true;
}

void UParticleModuleMeshRotationRate::Spawn(const FSpawnContext& Context)
{
	FParticleMeshEmitterInstance* MeshInst = dynamic_cast<FParticleMeshEmitterInstance*>(&Context.Owner);
	if (!MeshInst || MeshInst->MeshRotationOffset <= 0)
	{
		return;
	}
	FMeshRotationPayloadData* Payload = reinterpret_cast<FMeshRotationPayloadData*>(reinterpret_cast<uint8*>(Context.ParticleBase) + MeshInst->MeshRotationOffset);
	const FVector StartRate = StartRotationRate.GetValue(Context.Owner.EmitterTime, Context.GetDistributionData()) * 360.0f;
	Payload->RotationRateBase += StartRate;
	Payload->RotationRate += StartRate;
}

void UParticleModuleMeshRotationRate::Serialize(FArchive& Ar)
{
	UParticleModule::Serialize(Ar);
	StartRotationRate.Serialize(Ar);
}
