#include "Particles/Rotation/ParticleModuleMeshRotation.h"

#include "Particles/ParticleEmitterInstances.h"
#include "Serialization/Archive.h"

UParticleModuleMeshRotation::UParticleModuleMeshRotation()
	: bInheritParent(false)
{
	bSpawnModule = true;
	bUpdateModule = false;
}

void UParticleModuleMeshRotation::Spawn(const FSpawnContext& Context)
{
	FParticleMeshEmitterInstance* MeshInst = dynamic_cast<FParticleMeshEmitterInstance*>(&Context.Owner);
	if (!MeshInst || MeshInst->MeshRotationOffset <= 0)
	{
		return;
	}
	FMeshRotationPayloadData* Payload = reinterpret_cast<FMeshRotationPayloadData*>(reinterpret_cast<uint8*>(Context.ParticleBase) + MeshInst->MeshRotationOffset);
	Payload->InitRotation = StartRotation.GetValue(Context.Owner.EmitterTime, Context.GetDistributionData());
	Payload->Rotation = Payload->InitRotation;
}

void UParticleModuleMeshRotation::Serialize(FArchive& Ar)
{
	UParticleModule::Serialize(Ar);
	StartRotation.Serialize(Ar);
	bool InheritParent = bInheritParent;
	Ar << InheritParent;
	if (Ar.IsLoading())
	{
		bInheritParent = InheritParent;
	}
}
