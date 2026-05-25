#pragma once
#include "ParticleModuleSizeBase.h"
#include "Distributions/DistributionVector.h"
#include "Source/Engine/Particles/Size/ParticleModuleSize.generated.h"

UCLASS()
class UParticleModuleSize : public UParticleModuleSizeBase
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Size")
	FRawDistributionVector StartSize;

	virtual void Spawn(const FSpawnContext& Context) override;

	virtual void Serialize(FArchive& Ar) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
};
