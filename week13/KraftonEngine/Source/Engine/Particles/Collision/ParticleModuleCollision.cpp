#include "ParticleModuleCollision.h"
#include "Serialization/Archive.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleLODLevel.h"
#include "Component/Primitive/ParticleSystemComponent.h"
#include "GameFramework/World.h"
#include "Core/Logging/Log.h"
#include "Core/Types/CollisionTypes.h"

UParticleModuleCollision::UParticleModuleCollision()
{
	bUpdateModule = true;
}

void UParticleModuleCollision::Update(const FUpdateContext& Context)
{
	if (!Context.Owner.Component)
	{
		return;
	}

	UParticleLODLevel* LODLevel = Context.Owner.GetCurrentLODLevelChecked();
	UParticleModuleEventGenerator* EventGenerator = LODLevel->EventGenerator;
	const bool bEmitCollisionEvent = EventGenerator && EventGenerator->bEnabled && EventGenerator->bGenerateCollisionEvents;
	(void)bEmitCollisionEvent;

	BEGIN_UPDATE_LOOP
	{
		FVector NextLocation = Particle.Location + Particle.Velocity * Context.DeltaTime;
		FHitResult HitResult;
		if (PerformCollisionCheck(&Context.Owner, &Particle, HitResult, Context.Owner.Component->GetOwner(), Particle.Location, NextLocation))
		{
			if (bKillOnCollision)
			{
				Particle.RelativeTime = 1.0f;
			}
			else
			{
				FVector HitNormal = HitResult.ImpactNormal.IsNearlyZero() ? HitResult.WorldNormal : HitResult.ImpactNormal;
				HitNormal = HitNormal.GetSafeNormal(1.0e-6f, FVector::ZAxisVector);
				Particle.Velocity = (Particle.Velocity - HitNormal * (2.0f * Particle.Velocity.Dot(HitNormal))) * Restitution;
			}
		}
	}
	END_UPDATE_LOOP
}

void UParticleModuleCollision::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << Radius;
	Ar << Restitution;
	Ar << bKillOnCollision;
}

bool UParticleModuleCollision::PerformCollisionCheck(FParticleEmitterInstance* Owner, FBaseParticle* InParticle, FHitResult& OutHitResult, AActor* SourceActor, const FVector& Start, const FVector& End)
{
	UWorld* World = Owner->Component->GetWorld();
	AActor* Actor;
	bool bCollided = World->LinecastPrimitives(Start, End, OutHitResult, Actor);
	return bCollided;
}
