#include "Particles/Material/ParticleModuleMeshMaterial.h"
#include "Object/GarbageCollection.h"
#include "Materials/Material.h"
#include "Serialization/Archive.h"

UParticleModuleMeshMaterial::UParticleModuleMeshMaterial()
{
	bSpawnModule = false;
	bUpdateModule = false;
}

void UParticleModuleMeshMaterial::ResolveMaterials()
{
	// UE original responsibility: expose per-section material overrides for mesh particles.
	// Missing Jungle foundation: material slot resolution by static-mesh section.
	// System to connect later: StaticMesh section material lookup plus ParticleSystemComponent
	// material override array.
}

void UParticleModuleMeshMaterial::AddReferencedObjects(FReferenceCollector& Collector)
{
	UParticleModule::AddReferencedObjects(Collector);
	for (UMaterial* Material : MeshMaterials)
	{
		Collector.AddReferencedObject(Material, "UParticleModuleMeshMaterial.MeshMaterials");
	}
}

void UParticleModuleMeshMaterial::Serialize(FArchive& Ar)
{
	UParticleModule::Serialize(Ar);
	// UE original responsibility: serialize mesh material override object references.
	// Missing Jungle foundation: stable material soft-object array serializer for particle
	// modules.
	// System to connect later: FSoftObjectPtr array save/load and UMaterial resolve.
	if (Ar.IsLoading())
	{
		ResolveMaterials();
	}
}
