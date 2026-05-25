#pragma once
#include "ParticleModuleCollisionBase.h"

#include "Source/Engine/Particles/Collision/ParticleModuleCollision.generated.h"

struct FHitResult;
class AActor;

UCLASS()
class UParticleModuleCollision : public UParticleModuleCollisionBase
{
public:
	GENERATED_BODY()
	UParticleModuleCollision();

	float Radius = 1.0f;
	float Restitution = 0.5f;
	bool bKillOnCollision = false;

	virtual void Update(const FUpdateContext& Context) override;
	virtual void Serialize(FArchive& Ar) override;

	bool PerformCollisionCheck(FParticleEmitterInstance* Owner, FBaseParticle* InParticle, FHitResult& OutHitResult, AActor* SourceActor, const FVector& Start, const FVector& End);
};
