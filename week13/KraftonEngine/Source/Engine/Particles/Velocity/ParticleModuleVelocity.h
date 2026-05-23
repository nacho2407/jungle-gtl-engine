#pragma once
#include "ParticleModuleVelocityBase.h"

#include "Source/Engine/Particles/Velocity/ParticleModuleVelocity.generated.h"

UCLASS()
class UParticleModuleVelocity : public UParticleModuleVelocityBase
{
public:
	GENERATED_BODY()
	// FRawDistributionVector 는 나중에 추가
	UPROPERTY(EditAnywhere, Category = "Velocity")
	FVector MinVelocity;
	UPROPERTY(EditAnywhere, Category = "Velocity")
	FVector MaxVelocity;
	virtual void Spawn(const FSpawnContext& Context) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
};