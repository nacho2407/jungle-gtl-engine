#include "ProjectileMovementComponent.h"

#include "Component/SceneComponent.h"
#include "Component/DecalComponent.h"
#include "Component/StaticMeshComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Engine/Core/EngineTypes.h"
#include <Render/Culling/ConvexVolume.h>
#include <Core/RayTypes.h>
#include <UI/EditorConsoleWidget.h>
#include <GameFramework/EggActor.h>
#include <GameFramework/DecalActor.h>

IMPLEMENT_CLASS(UProjectileMovementComponent, UMovementComponent)

namespace {
	FBoundingBox BuildSweptAABB(const FBoundingBox & box, const FVector& Delta)
	{
        FBoundingBox Swept = box;
        Swept.Expand(box.Min + Delta);
        Swept.Expand(box.Max + Delta);
        return Swept;
    }
}

void UProjectileMovementComponent::BeginPlay()
{
	UMovementComponent::BeginPlay();

	if (Velocity.Length() <= FMath::Epsilon)
    {
        USceneComponent* SourceComponent = GetUpdatedComponent();
        if (!SourceComponent)
        {
            AActor* OwnerActor = GetOwner();
            SourceComponent = OwnerActor ? OwnerActor->GetRootComponent() : nullptr;
        }

        if (SourceComponent)
        {
            FVector Dir = SourceComponent->GetForwardVector().Normalized();
            Velocity = Dir * InitialSpeed;
        }
    }

	Velocity = Velocity.Normalized() * InitialSpeed; 
}

void UProjectileMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

    USceneComponent* UpdatedSceneComponent = GetUpdatedComponent();
    if (!UpdatedSceneComponent || Velocity.Length() <= FMath::Epsilon)
    {
        return;
    }

    const FVector GravityAccel = FVector(0.f, 0.f, -1.f) * GravityScale * 9.8f;

    Velocity += GravityAccel * DeltaTime;

    const float CurrentSpeed = Velocity.Length();
    if (MaxSpeed > 0.0f && CurrentSpeed > MaxSpeed)
    {
        Velocity = Velocity.Normalized() * MaxSpeed;
    }

    const FVector MoveDelta = Velocity * DeltaTime;
    if (MoveDelta.Length() <= FMath::Epsilon)
    {
        return;
    }

	const FVector Start = UpdatedSceneComponent->GetWorldLocation();

	FHitResult Hit;
	if (CheckBlockingHit(UpdatedSceneComponent, Start, MoveDelta, Hit))
	{
		const float SafeTime = std::max(0.0f, Hit.Time - 0.001f);
		UpdatedSceneComponent->SetWorldLocation(Start + MoveDelta * SafeTime);
		HandleBlockingHit(UpdatedSceneComponent, Start, MoveDelta, Hit);
		return;
	}

    UpdatedSceneComponent->SetWorldLocation(UpdatedSceneComponent->GetWorldLocation() + MoveDelta);
}

void UProjectileMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UMovementComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Velocity", EPropertyType::Vec3, &Velocity, 0.0f, 0.0f, 1.0f });
	OutProps.push_back({ "Initial Speed", EPropertyType::Float, &InitialSpeed, 0.0f, MaxSpeed, 5.0f });
	OutProps.push_back({ "Max Speed", EPropertyType::Float, &MaxSpeed, 0.0f, 1000.0f, 10.0f });
	OutProps.push_back({ "Gravity Scale", EPropertyType::Float, &GravityScale, 0.0f, 1.0f, 0.01f });
}

void UProjectileMovementComponent::Serialize(FArchive& Ar)
{
	UMovementComponent::Serialize(Ar);
	Ar << Velocity;
	Ar << InitialSpeed;
	Ar << MaxSpeed;
	Ar << GravityScale;
}

void UProjectileMovementComponent::StopSimulating()
{
	SetComponentTickEnabled(false);
	Velocity = FVector(0.0f, 0.0f, 0.0f);
}

FVector UProjectileMovementComponent::GetPreviewVelocity() const
{
	return ComputeEffectiveVelocity();
}

EProjectileHitBehavior UProjectileMovementComponent::GetHitBehavior() const
{
	return EProjectileHitBehavior::Stop;
}

FVector UProjectileMovementComponent::ComputeEffectiveVelocity() const
{
	if (Velocity.Length() > FMath::Epsilon)
    {
        return Velocity;
    }

    USceneComponent* SourceComponent = GetUpdatedComponent();
    if (!SourceComponent)
    {
        AActor* OwnerActor = GetOwner();
        SourceComponent = OwnerActor ? OwnerActor->GetRootComponent() : nullptr;
    }

    if (SourceComponent)
    {
        return SourceComponent->GetForwardVector().Normalized() * InitialSpeed;
    }

    return FVector();
}

bool UProjectileMovementComponent::HandleBlockingHit(USceneComponent* UpdatedSceneComponent, const FVector& CurrentLocation, const FVector& MoveDelta, const FHitResult& HitResult)
{
	(void)UpdatedSceneComponent;
	(void)CurrentLocation;
	(void)MoveDelta;
	
	UWorld* World = GetOwner() ? GetOwner()->GetWorld() : nullptr;
	StopSimulating();

	AEggActor* Egg = Cast<AEggActor>(Owner);

    if (Egg)
    {
        if (UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Egg->GetRootComponent()))
		{
			Mesh->SetVisibility(false);
		}

		UDecalComponent* Decal = nullptr;
		for (UActorComponent* Comp : Egg->GetComponents())
		{
			Decal = Cast<UDecalComponent>(Comp);
			if (Decal)
			{
				break;
			}
		}

		if (!Decal)
		{
			Decal = Egg->AddComponent<UDecalComponent>();
			if (USceneComponent* Root = Egg->GetRootComponent())
			{
				Decal->AttachToComponent(Root);
			}
		}

		Decal->SetTexture("Friedegg");
		Decal->SetWorldLocation(HitResult.WorldHitLocation);

		FFadeSetting Setting{};
		Setting.bFadeOnceAndDestroy = true;
		Setting.FadeInTime = 0.1f;
		Setting.FadeOutTime = 0.1f;
		Setting.TotalLifetime = 4.0f;
		Decal->SetFadeSetting(Setting);
    }

	return true;
}

bool UProjectileMovementComponent::CheckBlockingHit(USceneComponent* UpdatedSceneComponent, const FVector& CurrentLocation, const FVector& MoveDelta, FHitResult& OutHitResult)
{
	UWorld* World = GetOwner() ? GetOwner()->GetWorld() : nullptr;
    UPrimitiveComponent* UpdatedPrimitive = Cast<UPrimitiveComponent>(UpdatedSceneComponent);
    if (!World || !UpdatedPrimitive)
    {
        return false;
    }
	 
	const float MoveDistance = MoveDelta.Length();
    if (MoveDistance <= FMath::Epsilon)
    {
        return false;
    }

	const FVector Direction = MoveDelta / MoveDistance;
    const FRay SweepRay{ CurrentLocation, Direction };

    TArray<UPrimitiveComponent*> Candidates;
    const FBoundingBox SweptBounds = BuildSweptAABB(UpdatedPrimitive->GetWorldBoundingBox(), MoveDelta);
    World->GetPartition().QueryAABBPrimitive(SweptBounds, Candidates);

    bool bFoundHit = false;
    float ClosestTime = 1.0f;

	for (UPrimitiveComponent* Candidate : Candidates)
    {
        if (!Candidate)
        {
            continue;
        }

        if (Candidate == UpdatedPrimitive)
        {
            continue;
        }

        if (Candidate->GetOwner() == GetOwner())
        {
            continue;
        }

        if (!Candidate->IsVisible())
        {
            continue;
        }

        FHitResult CandidateHit{};
        if (!Candidate->LineTraceComponent(SweepRay, CandidateHit))
        {
            continue;
        }

        if (CandidateHit.Distance < 0.0f || CandidateHit.Distance > MoveDistance)
        {
            continue;
        }

        const float HitTime = CandidateHit.Distance / MoveDistance;
        if (HitTime >= ClosestTime)
        {
            continue;
        }

        ClosestTime = HitTime;
        OutHitResult = CandidateHit;
        OutHitResult.bHit = true;
        OutHitResult.Time = HitTime;
        OutHitResult.HitComponent = Candidate;

        const FVector ImpactPoint = CurrentLocation + Direction * CandidateHit.Distance;
        OutHitResult.WorldHitLocation = ImpactPoint;

        bFoundHit = true;
    }

    return bFoundHit;
}