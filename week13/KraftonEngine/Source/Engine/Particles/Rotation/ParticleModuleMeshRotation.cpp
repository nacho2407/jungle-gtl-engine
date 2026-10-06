#include "Particles/Rotation/ParticleModuleMeshRotation.h"
#include "Object/GarbageCollection.h"

#include "Component/Primitive/ParticleSystemComponent.h"
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
	FVector Rotation = StartRotation.GetValue(Context.Owner.EmitterTime, Context.GetDistributionData());
	if (bInheritParent && Context.Owner.Component)
	{
		const FVector ParentAffectedRotation = Context.Owner.Component->GetWorldRotation().ToVector();
		Rotation.X += ParentAffectedRotation.X / 360.0f;
		Rotation.Y += ParentAffectedRotation.Y / 360.0f;
		Rotation.Z += ParentAffectedRotation.Z / 360.0f;
	}
	Payload->InitRotation = Rotation * 360.0f;
	Payload->Rotation += Payload->InitRotation;
}

void UParticleModuleMeshRotation::AddReferencedObjects(FReferenceCollector& Collector)
{
	UParticleModule::AddReferencedObjects(Collector);
	StartRotation.AddReferencedObjects(Collector);
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
