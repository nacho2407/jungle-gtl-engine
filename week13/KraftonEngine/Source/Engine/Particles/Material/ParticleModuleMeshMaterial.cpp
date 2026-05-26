#include "Particles/Material/ParticleModuleMeshMaterial.h"

#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Serialization/Archive.h"

UParticleModuleMeshMaterial::UParticleModuleMeshMaterial()
{
	bSpawnModule = false;
	bUpdateModule = false;
}

void UParticleModuleMeshMaterial::ResolveMaterials()
{
	MeshMaterials.clear();
	MeshMaterials.resize(MeshMaterialSlots.size(), nullptr);
	for (int32 Index = 0; Index < static_cast<int32>(MeshMaterialSlots.size()); ++Index)
	{
		const FString Path = MeshMaterialSlots[Index].ToString();
		if (Path.empty() || Path == "None")
		{
			MeshMaterialSlots[Index] = "None";
			MeshMaterials[Index] = nullptr;
			continue;
		}

		UMaterial* Material = FMaterialManager::Get().GetOrCreateMaterial(Path);
		if (!Material)
		{
			MeshMaterialSlots[Index] = "None";
		}
		MeshMaterials[Index] = Material;
	}
}

void UParticleModuleMeshMaterial::Serialize(FArchive& Ar)
{
	UParticleModule::Serialize(Ar);

	int32 Version = 0;
	Ar << Version;

	uint32 SlotCount = Ar.IsSaving() ? static_cast<uint32>(MeshMaterialSlots.size()) : 0;
	Ar << SlotCount;
	if (Ar.IsLoading())
	{
		MeshMaterialSlots.clear();
		MeshMaterialSlots.resize(SlotCount);
	}

	for (uint32 SlotIdx = 0; SlotIdx < SlotCount; ++SlotIdx)
	{
		FString SlotPath = Ar.IsSaving() ? MeshMaterialSlots[SlotIdx].ToString() : FString("None");
		Ar << SlotPath;
		if (Ar.IsLoading())
		{
			MeshMaterialSlots[SlotIdx] = SlotPath;
		}
	}

	if (Ar.IsLoading())
	{
		ResolveMaterials();
	}
}
