#pragma once
#include "ParticleModuleVelocityBase.h"

#include "Distributions/DistributionVector.h"
#include "Distributions/DistributionFloat.h"

#include "Source/Engine/Particles/Velocity/ParticleModuleVelocity.generated.h"

UCLASS()
class UParticleModuleVelocity : public UParticleModuleVelocityBase
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Velocity)
	struct FRawDistributionVector StartVelocity;

	UPROPERTY(EditAnywhere, Category = Velocity)
	struct FRawDistributionFloat StartVelocityRadial;

	virtual void Spawn(const FSpawnContext& Context) override;

	virtual void Serialize(FArchive& Ar) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
};