#pragma once

#include "Distributions/DistributionVector.h"
#include "Particles/ParticleModule.h"

#include "Source/Engine/Particles/RotationRate/ParticleModuleMeshRotationRate.generated.h"

UCLASS()
class UParticleModuleMeshRotationRate : public UParticleModule
{
public:
	GENERATED_BODY()

	FRawDistributionVector StartRotationRate;

	UParticleModuleMeshRotationRate();
	void Spawn(const FSpawnContext& Context) override;
	void Serialize(FArchive& Ar) override;
};
