#include "ParticleModuleCollision.h"

#include "Component/Particle/ParticleSystemComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Particles/ParticleHelper.h"

void UParticleModuleCollision::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	if (!bEnabled || !Owner || DeltaTime <= 0.0f)
	{
		return;
	}

	UParticleSystemComponent* Component = Owner->GetComponent();
	UWorld* World = Component ? Component->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	AActor* IgnoreActor = Component ? Component->GetOwner() : nullptr;

	struct
	{
		FParticleEmitterInstance& Owner;
		int32 Offset;
		float DeltaTime;
	} Context{ *Owner, Offset, DeltaTime };

	BEGIN_UPDATE_LOOP
		const FVector Move = Particle->Position - Particle->OldPosition;
		const float MoveDistance = Move.Length();
		if (MoveDistance <= 1.0e-4f)
		{
			continue;
		}

		FVector Direction = Move / MoveDistance;
		FHitResult Hit;
		if (!World->PhysicsRaycast(Particle->OldPosition, Direction, MoveDistance, Hit, TraceChannel, IgnoreActor))
		{
			continue;
		}

		FVector Normal = Hit.WorldNormal;
		if (Normal.Length() <= 1.0e-4f)
		{
			Normal = Hit.ImpactNormal;
		}
		if (Normal.Length() <= 1.0e-4f)
		{
			Normal = FVector::UpVector;
		}
		Normal.Normalize();

		Particle->Position = Hit.WorldHitLocation + Normal * Radius;

		if (Response == EParticleCollisionResponse::Kill)
		{
			Particle->bAlive = false;
			continue;
		}

		if (Response == EParticleCollisionResponse::Stop)
		{
			Particle->Velocity = FVector::ZeroVector;
			continue;
		}

		const float NormalSpeed = Particle->Velocity.Dot(Normal);
		FVector NormalVelocity = Normal * NormalSpeed;
		FVector TangentVelocity = Particle->Velocity - NormalVelocity;

		float ClampedRestitution = Restitution;
		if (ClampedRestitution < 0.0f)
		{
			ClampedRestitution = 0.0f;
		}

		float ClampedFriction = Friction;
		if (ClampedFriction < 0.0f)
		{
			ClampedFriction = 0.0f;
		}
		else if (ClampedFriction > 1.0f)
		{
			ClampedFriction = 1.0f;
		}

		NormalVelocity = NormalVelocity * -ClampedRestitution;
		TangentVelocity = TangentVelocity * (1.0f - ClampedFriction);
		Particle->Velocity = NormalVelocity + TangentVelocity;
	END_UPDATE_LOOP
}
