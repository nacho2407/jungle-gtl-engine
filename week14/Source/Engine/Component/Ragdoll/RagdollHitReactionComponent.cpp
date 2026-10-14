#include "RagdollHitReactionComponent.h"

#include "Component/Movement/PhysX/VehicleMovementComponent4W.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Component/Shape/BoxComponent.h"
#include "Core/Logging/Log.h"
#include "Core/Types/CollisionTypes.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/Matrix.h"

#include <algorithm>
#include <cmath>

void URagdollHitReactionComponent::BeginPlay()
{
	UActorComponent::BeginPlay();

	AActor* OwnerActor = GetOwner();
	CachedMesh = OwnerActor ? OwnerActor->GetComponentByClass<USkeletalMeshComponent>() : nullptr;
	if (!CachedMesh)
	{
		UE_LOG("[RagdollHitReaction] SkeletalMeshComponent not found.");
		return;
	}

	CreateRuntimeHitBox();
}

void URagdollHitReactionComponent::EndPlay()
{
	DestroyRuntimeHitBox(false);
	CachedMesh = nullptr;
	CachedVehicleActor = nullptr;
	bHasPreviousVehicleLocation = false;

	UActorComponent::EndPlay();
}

void URagdollHitReactionComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bUseVehicleProximityCheck || bTriggered || !CachedMesh || CachedMesh->IsSimulatingPhysics())
	{
		return;
	}

	AActor* VehicleActor = ResolveVehicleActor();
	if (!VehicleActor)
	{
		return;
	}

	FVector VehicleVelocity = FVector::ZeroVector;
	if (!IsVehicleTouchingHitBox(VehicleActor, DeltaTime, VehicleVelocity))
	{
		return;
	}

	if (VehicleVelocity.Length() < MinVehicleSpeedToTrigger)
	{
		return;
	}

	const FVector LaunchVelocity = ComputeLaunchVelocityFromVehicle(VehicleActor, VehicleVelocity);
	TriggerRagdoll(VehicleActor, LaunchVelocity, "VehicleProbe");
}

void URagdollHitReactionComponent::CreateRuntimeHitBox()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || RuntimeHitBox)
	{
		return;
	}

	RuntimeHitBox = OwnerActor->AddComponent<UBoxComponent>();
	if (!RuntimeHitBox)
	{
		return;
	}

	RuntimeHitBox->SetHiddenInComponentTree(true);
	RuntimeHitBox->SetVisibility(false);

	if (USceneComponent* Root = OwnerActor->GetRootComponent())
	{
		RuntimeHitBox->AttachToComponent(Root);
	}

	RuntimeHitBox->SetBoxExtent(HitBoxExtent);
	RuntimeHitBox->SetRelativeLocation(HitBoxOffset);

	RuntimeHitBox->SetGenerateOverlapEvents(false);
	RuntimeHitBox->SetSimulatePhysics(false);
	RuntimeHitBox->SetEnableGravity(false);
	RuntimeHitBox->SetCollisionObjectType(ECollisionChannel::Pawn);
	RuntimeHitBox->SetCollisionResponseToAllChannels(ECollisionResponse::Block);
	RuntimeHitBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	RuntimeHitBox->CreatePhysicsState();

	HitHandle = RuntimeHitBox->OnComponentHit.AddRaw(this, &URagdollHitReactionComponent::HandleHit);

	UE_LOG("[RagdollHitReaction] Runtime hit box created.");
}

void URagdollHitReactionComponent::DestroyRuntimeHitBox(bool bRemoveFromOwner)
{
	if (!RuntimeHitBox)
	{
		return;
	}

	if (HitHandle.IsValid())
	{
		RuntimeHitBox->OnComponentHit.Remove(HitHandle);
		HitHandle.Reset();
	}

	RuntimeHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (bRemoveFromOwner)
	{
		if (AActor* OwnerActor = GetOwner())
		{
			OwnerActor->RemoveComponent(RuntimeHitBox);
		}
	}

	RuntimeHitBox = nullptr;
}

bool URagdollHitReactionComponent::ShouldReactToActor(AActor* OtherActor) const
{
	if (!OtherActor)
	{
		return false;
	}

	if (!VehicleActorName.empty() && OtherActor->GetFName().ToString() != VehicleActorName)
	{
		return false;
	}

	if (bRequireVehicleMovementComponent && !OtherActor->GetComponentByClass<UVehicleMovementComponent4W>())
	{
		return false;
	}

	return true;
}

AActor* URagdollHitReactionComponent::ResolveVehicleActor()
{
	if (CachedVehicleActor && ShouldReactToActor(CachedVehicleActor))
	{
		return CachedVehicleActor;
	}

	CachedVehicleActor = nullptr;
	bHasPreviousVehicleLocation = false;

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (AActor* Actor : World->GetActors())
	{
		if (ShouldReactToActor(Actor))
		{
			CachedVehicleActor = Actor;
			PreviousVehicleLocation = Actor->GetActorLocation();
			bHasPreviousVehicleLocation = true;
			UE_LOG(
				"[RagdollHitReaction] Vehicle probe target found: %s",
				CachedVehicleActor->GetName().c_str());
			return CachedVehicleActor;
		}
	}

	return nullptr;
}

bool URagdollHitReactionComponent::IsVehicleTouchingHitBox(
	AActor* VehicleActor,
	float DeltaTime,
	FVector& OutVehicleVelocity)
{
	OutVehicleVelocity = FVector::ZeroVector;

	if (!VehicleActor)
	{
		return false;
	}

	USceneComponent* VehicleRoot = VehicleActor->GetRootComponent();
	if (!VehicleRoot)
	{
		return false;
	}

	const FVector CurrentVehicleLocation = VehicleActor->GetActorLocation();
	if (bHasPreviousVehicleLocation && DeltaTime > 1.e-6f)
	{
		OutVehicleVelocity = (CurrentVehicleLocation - PreviousVehicleLocation) / DeltaTime;
	}

	const float SweepPadding = std::clamp(OutVehicleVelocity.Length() * std::max(DeltaTime, 0.0f), 0.0f, MaxVehicleSweepPadding);

	const FVector HitCenter = RuntimeHitBox
		? RuntimeHitBox->GetWorldLocation()
		: (GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector);

	const FVector HitExtent = RuntimeHitBox
		? RuntimeHitBox->GetScaledBoxExtent()
		: HitBoxExtent;

	const FVector LocalHitCenter = VehicleRoot->GetWorldMatrix().GetInverse().TransformPositionWithW(HitCenter);
	const FVector ExpandedVehicleExtent(
		std::max(0.0f, VehicleHalfExtent.X + std::abs(HitExtent.X) + VehicleHitPadding + SweepPadding),
		std::max(0.0f, VehicleHalfExtent.Y + std::abs(HitExtent.Y) + VehicleHitPadding + SweepPadding),
		std::max(0.0f, VehicleHalfExtent.Z + std::abs(HitExtent.Z) + VehicleHitPadding));

	const bool bTouching = std::abs(LocalHitCenter.X) <= ExpandedVehicleExtent.X
		&& std::abs(LocalHitCenter.Y) <= ExpandedVehicleExtent.Y
		&& std::abs(LocalHitCenter.Z) <= ExpandedVehicleExtent.Z;

	PreviousVehicleLocation = CurrentVehicleLocation;
	bHasPreviousVehicleLocation = true;

	return bTouching;
}

FVector URagdollHitReactionComponent::ComputeLaunchVelocity(
	AActor* OtherActor,
	const FVector& NormalImpulse,
	const FHitResult& HitResult) const
{
	AActor* OwnerActor = GetOwner();

	FVector LaunchDirection = OwnerActor && OtherActor
		? OwnerActor->GetActorLocation() - OtherActor->GetActorLocation()
		: HitResult.ImpactNormal;

	LaunchDirection.Z = 0.0f;

	if (LaunchDirection.Length() <= 1.e-3f)
	{
		LaunchDirection = HitResult.ImpactNormal;
		LaunchDirection.Z = 0.0f;
	}

	if (LaunchDirection.Length() <= 1.e-3f)
	{
		LaunchDirection = FVector::ForwardVector;
	}

	LaunchDirection.Normalize();

	const float SpeedFromImpulse = NormalImpulse.Length() * NormalImpulseToVelocityScale;
	const float MaxSpeed = std::max(MaxLaunchSpeed, MinLaunchSpeed);
	const float HorizontalSpeed = std::clamp(std::max(MinLaunchSpeed, SpeedFromImpulse), 0.0f, MaxSpeed);

	return LaunchDirection * HorizontalSpeed + FVector::UpVector * UpwardLaunchVelocity;
}

FVector URagdollHitReactionComponent::ComputeLaunchVelocityFromVehicle(
	AActor* VehicleActor,
	const FVector& VehicleVelocity) const
{
	FVector LaunchDirection = VehicleVelocity;
	LaunchDirection.Z = 0.0f;

	if (LaunchDirection.Length() <= 1.e-3f)
	{
		AActor* OwnerActor = GetOwner();
		LaunchDirection = OwnerActor && VehicleActor
			? OwnerActor->GetActorLocation() - VehicleActor->GetActorLocation()
			: FVector::ForwardVector;
		LaunchDirection.Z = 0.0f;
	}

	if (LaunchDirection.Length() <= 1.e-3f)
	{
		LaunchDirection = FVector::ForwardVector;
	}

	LaunchDirection.Normalize();

	const float MaxSpeed = std::max(MaxLaunchSpeed, MinLaunchSpeed);
	const float HorizontalSpeed = std::clamp(std::max(MinLaunchSpeed, VehicleVelocity.Length()), 0.0f, MaxSpeed);
	return LaunchDirection * HorizontalSpeed + FVector::UpVector * UpwardLaunchVelocity;
}

void URagdollHitReactionComponent::TriggerRagdoll(AActor* OtherActor, const FVector& LaunchVelocity, const char* SourceName)
{
	if (bTriggered || !CachedMesh || CachedMesh->IsSimulatingPhysics())
	{
		return;
	}

	CachedMesh->SetPhysicsBlendWeight(1.0f);
	CachedMesh->StartRagdollWithVelocity(LaunchVelocity);

	if (!CachedMesh->IsSimulatingPhysics())
	{
		UE_LOG("[RagdollHitReaction] Ragdoll start failed.");
		return;
	}

	bTriggered = true;

	if (bDisableHitBoxAfterTrigger && RuntimeHitBox)
	{
		RuntimeHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	UE_LOG(
		"[RagdollHitReaction] Vehicle ragdoll triggered. Source=%s Other=%s Launch=(%.2f, %.2f, %.2f)",
		SourceName ? SourceName : "Unknown",
		OtherActor ? OtherActor->GetName().c_str() : "None",
		LaunchVelocity.X,
		LaunchVelocity.Y,
		LaunchVelocity.Z);
}

void URagdollHitReactionComponent::HandleHit(
	UPrimitiveComponent* /*HitComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	FVector NormalImpulse,
	const FHitResult& HitResult)
{
	if (bTriggered || !CachedMesh || CachedMesh->IsSimulatingPhysics())
	{
		return;
	}

	if (!ShouldReactToActor(OtherActor))
	{
		return;
	}

	const FVector LaunchVelocity = ComputeLaunchVelocity(OtherActor, NormalImpulse, HitResult);
	TriggerRagdoll(OtherActor, LaunchVelocity, "PhysicsHit");
}
