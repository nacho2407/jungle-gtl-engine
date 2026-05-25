#pragma once
#include "ParticleModuleCollisionBase.h"

#include "Source/Engine/Particles/Collision/ParticleModuleCollision.generated.h"

UCLASS()
class UParticleModuleCollision : public UParticleModuleCollisionBase
{
public:
	GENERATED_BODY()

	float Radius = 1.0f;
	float Restitution = 0.5f;
	bool bKillOnCollision = false;

	virtual void Update(const FUpdateContext& Context) override;
	virtual void Serialize(FArchive& Ar) override;
};
