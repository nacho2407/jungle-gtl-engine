#pragma once

#include "Object/Ptr/SoftObjectPtr.h"
#include "Particles/ParticleModule.h"

class UMaterial;

#include "Source/Engine/Particles/Material/ParticleModuleMeshMaterial.generated.h"

UCLASS()
class UParticleModuleMeshMaterial : public UParticleModule
{
public:
	GENERATED_BODY()

	TArray<FSoftObjectPtr> MeshMaterialSlots;
	TArray<UMaterial*> MeshMaterials;

	UParticleModuleMeshMaterial();
	EModuleType GetModuleType() const override { return EPMT_General; }
	void Serialize(FArchive& Ar) override;
	void ResolveMaterials();
};
