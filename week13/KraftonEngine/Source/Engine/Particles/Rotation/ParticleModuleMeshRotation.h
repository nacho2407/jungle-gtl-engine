#pragma once

#include "Distributions/DistributionVector.h"
#include "Particles/ParticleModule.h"

#include "Source/Engine/Particles/Rotation/ParticleModuleMeshRotation.generated.h"

UCLASS()
class UParticleModuleMeshRotation : public UParticleModule
{
public:
	GENERATED_BODY()

	FRawDistributionVector StartRotation;
	uint32 bInheritParent : 1;

	UParticleModuleMeshRotation();
	void Spawn(const FSpawnContext& Context) override;
	void Serialize(FArchive& Ar) override;
	void AddReferencedObjects(FReferenceCollector& Collector) override;
};
