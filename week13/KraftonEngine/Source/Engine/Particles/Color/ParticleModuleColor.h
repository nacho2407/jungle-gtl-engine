#pragma once
#include "ParticleModuleColorBase.h"
#include "Core/Types/EngineTypes.h"
#include "Distributions/DistributionVector.h"
#include "Distributions/DistributionFloat.h"
#include "Source/Engine/Particles/Color/ParticleModuleColor.generated.h"

UCLASS()
class UParticleModuleColor : public UParticleModuleColorBase
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Color")
	FRawDistributionVector StartColor;

	UPROPERTY(EditAnywhere, Category = "Color")
	FRawDistributionFloat StartAlpha;

	uint8 bClampAlpha : 1;

	virtual void Spawn(const FSpawnContext& Context) override;

	virtual void Serialize(FArchive& Ar) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

#if WITH_EDITOR
	virtual	void PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
};
