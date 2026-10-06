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

		if (PerformCollisionCheck(&Context.Owner, &Particle, HitResult,
			Context.Owner.Component->GetOwner(),
			Particle.Location, NextLocation))
		{
			if (bKillOnCollision)
			{
				Particle.RelativeTime = 1.0f;  // 수명 만료 처리
				Context.Owner.KillParticle(CurrentIndex);
			}
			else
			{
				// 반사
				Particle.Location = HitResult.WorldHitLocation + HitResult.ImpactNormal * 0.5f;

				FVector Reflected = Particle.Velocity -
					HitResult.ImpactNormal * 2.f * (Particle.Velocity.Dot(HitResult.ImpactNormal));
				Particle.Velocity = Reflected * Restitution;
				Particle.BaseVelocity = Particle.Velocity;
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
	FVector Diff = End - Start;
	float Dist = Diff.Length();

	FCollisionShape BoxShape = FCollisionShape::MakeBox(InParticle->Size * 0.5f);

	bool bCollided = World->PhysicsSweep(
		Start, Diff / Dist, Dist,
		BoxShape, FQuat::Identity,
		OutHitResult,
		ECollisionChannel::WorldStatic,
		SourceActor
	);

	if (bCollided)
	{
		// penetrating이면 충돌 없는 것으로 처리 (위치 보정 하지 않음)
		if (OutHitResult.bStartPenetrating || OutHitResult.Distance <= 0.f)
			return false;
	}

	return bCollided;
}
