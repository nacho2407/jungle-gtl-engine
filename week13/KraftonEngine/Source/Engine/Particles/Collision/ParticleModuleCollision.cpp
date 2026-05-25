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
	UParticleLODLevel* LODLevel = Context.Owner.GetCurrentLODLevelChecked();
	UParticleModuleEventGenerator* EventGenerator = LODLevel->EventGenerator;

	if (!EventGenerator || !EventGenerator->bGenerateCollisionEvents)
		return;

	BEGIN_UPDATE_LOOP
	{
		FVector NextLocation = Particle.Location + Particle.Velocity * Context.DeltaTime;
		FHitResult HitResult;
		if (PerformCollisionCheck(&Context.Owner, &Particle, HitResult, Context.Owner.Component->GetOwner(), Particle.Location, NextLocation))
		{
			UE_LOG("Hit");
			if (bKillOnCollision)
			{
				Particle.RelativeTime = 1.0f;
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
