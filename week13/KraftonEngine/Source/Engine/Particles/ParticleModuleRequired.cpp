#include "Particles/ParticleModuleRequired.h"

#include "Materials/Material.h"
#include "Materials/MaterialManager.h"

#include <cstring>

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
