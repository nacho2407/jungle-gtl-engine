#pragma once
#include "ParticleModuleColorBase.h"
#include "Distributions/DistributionVector.h"
#include "Distributions/DistributionFloat.h"

#include "Source/Engine/Particles/Color/ParticleModuleColorOverLife.generated.h"

UCLASS()
class UParticleModuleColorOverLife : public UParticleModuleColorBase
{
public:
	GENERATED_BODY()
	UParticleModuleColorOverLife();

	/** The color to apply to the particle, as a function of the particle RelativeTime. */
	UPROPERTY(EditAnywhere, Category = "Color")
	FRawDistributionVector ColorOverLife;

	/** The alpha to apply to the particle, as a function of the particle RelativeTime. */
	UPROPERTY(EditAnywhere, Category = "Color")
	FRawDistributionFloat AlphaOverLife;

	/** If true, the alpha value will be clamped to the [0..1] range. */
	uint8 bClampAlpha : 1;

	virtual void Spawn(const FSpawnContext& Context) override;
	virtual void Update(const FUpdateContext& Context) override;

	virtual void Serialize(FArchive& Ar) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

};
