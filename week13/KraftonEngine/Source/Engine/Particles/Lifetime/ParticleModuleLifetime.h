#pragma once
#include "ParticleModuleLifetimeBase.h"

#include "Source/Engine/Particles/Lifetime/ParticleModuleLifetime.generated.h"

UCLASS()
class UParticleModuleLifetime : public UParticleModuleLifetimeBase
{
public:
	GENERATED_BODY()
	// FRawDistributionFloat 는 나중에 추가

	UPROPERTY(EditAnywhere, Category = "Lifetime")
	float LifetimeMin = 1.0f;
	UPROPERTY(EditAnywhere, Category = "Lifetime")
	float LifetimeMax = 1.0f;

	/*
	Spawn
		Particle->Lifetime =
		RandomRange(LifetimeMin, LifetimeMax);
		Particle->RelativeTime = 0.0f;
	*/

	virtual void Spawn(const FSpawnContext& Context) override;

	//~ Begin UParticleModuleLifetimeBase Interface
	virtual float GetMaxLifetime() override;
	float GetLifetimeValue(const FContext& Context, float InTime, UObject* Data = NULL) override;
	//~ End UParticleModuleLifetimeBase Interface

#if WITH_EDITOR
	virtual void	PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
};