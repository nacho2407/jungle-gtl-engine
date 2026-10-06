#include "Particles/ParticleModuleRequired.h"

#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Serialization/Archive.h"

#include <cstring>

#include "Object/GarbageCollection.h"

void UParticleModuleRequired::SetMaterial(UMaterial* InMaterial)
{
	Material = InMaterial;
	MaterialSlot = Material ? Material->GetAssetPathFileName() : "None";
}

void UParticleModuleRequired::ResolveMaterialFromSlot()
{
	if (MaterialSlot.IsNull() || MaterialSlot == "None" || MaterialSlot.empty())
	{
		Material = nullptr;
		MaterialSlot = "None";
		return;
	}

	Material = FMaterialManager::Get().GetOrCreateMaterial(MaterialSlot.ToString());
	if (!Material)
	{
		MaterialSlot = "None";
	}
}

void UParticleModuleRequired::PostEditProperty(const char* PropertyName)
{
	UParticleModule::PostEditProperty(PropertyName);

	if (std::strcmp(PropertyName, "Material") == 0 ||
		std::strcmp(PropertyName, "MaterialSlot") == 0)
	{
		ResolveMaterialFromSlot();
	}
}

void UParticleModuleRequired::Serialize(FArchive& Ar)
{
	UParticleModule::Serialize(Ar);

	int32 Version = 0;
	Ar << Version;

	FString SlotPath = Ar.IsSaving() ? MaterialSlot.ToString() : FString();
	Ar << SlotPath;
	if (Ar.IsLoading())
	{
		MaterialSlot = SlotPath;
	}

	Ar << EmitterOrigin;
	Ar << EmitterRotation;

	bool bLocal = bUseLocalSpace;
	bool bKD    = bKillOnDeactivate;
	bool bKC    = bKillOnCompleted;
	Ar << bLocal;
	Ar << bKD;
	Ar << bKC;
	if (Ar.IsLoading())
	{
		bUseLocalSpace    = bLocal ? 1 : 0;
		bKillOnDeactivate = bKD    ? 1 : 0;
		bKillOnCompleted  = bKC    ? 1 : 0;
	}

	Ar << EmitterDuration;
	Ar << EmitterDurationLow;
	Ar << EmitterDelay;
	Ar << EmitterLoops;
	Ar << bDelayFirstLoopOnly;

	int32 SA = static_cast<int32>(ScreenAlignment);
	int32 SM = static_cast<int32>(SortMode);
	Ar << SA;
	Ar << SM;
	if (Ar.IsLoading())
	{
		ScreenAlignment = static_cast<EParticleScreenAlignment>(SA);
		SortMode        = static_cast<EParticleSortMode>(SM);
	}

	Ar << SubImages_Horizontal;
	Ar << SubImages_Vertical;

	Ar << SpawnRate;
	Ar << BurstList;          // TArray<FParticleBurst> — trivially copyable, fast path.

	Ar << bUseMaxDrawCount;
	Ar << MaxDrawCount;

	if (Ar.IsLoading())
	{
		ResolveMaterialFromSlot();
	}
}

void UParticleModuleRequired::AddReferencedObjects(FReferenceCollector& Collector)
{
    UParticleModule::AddReferencedObjects(Collector);

    Collector.AddReferencedObject(Material);
}
