#include "CharacterMovementComponent.h"

#include "Audio/AudioManager.h"
#include "Animation/AnimInstance.h"
#include "Component/Shape/CapsuleComponent.h"
#include "Component/SceneComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/Primitive/StaticMeshComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Collision/Ray/RayUtils.h"
#include "Core/Types/PropertyTypes.h"
#include "Core/TickFunction.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Pawn/Character.h"
#include "GameFramework/World.h"
#include "Core/Logging/Log.h"
#include "Math/MathUtils.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Physics/BodyInstance.h"
#include "Physics/PhysicsScene.h"
#include "Physics/PhysXSDK.h"
#include "Physics/PhysXConversions.h"
#include "Physics/PhysicsQueryFilter.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Render/Scene/FScene.h"
#include "Mesh/Static/StaticMesh.h"
#include "Mesh/Static/StaticMeshAsset.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>

namespace
{
	constexpr float MinWallRunInputAlong = 0.1f;
	constexpr float SprintFootstepMinSpeed = 1.0f;
	constexpr float WalkFootstepStrideDistance = 2.1f;
	constexpr float SprintFootstepStrideDistance = 2.6f;
	constexpr float SprintFootstepInitialDistanceRatio = 0.45f;
	constexpr float SlideAudioMinSpeed = 3.0f;
	constexpr float SlideStepStrideDistance = 2.4f;
	constexpr float WallRunStepStrideDistance = 2.2f;
	constexpr float WallRunLoopStartDelay = 0.42f;
	constexpr float HeavyLandDownSpeed = 9.0f;

	FBoundingBox ExpandBounds(const FBoundingBox& Bounds, float Amount)
	{
		if (!Bounds.IsValid() || Amount <= 0.0f)
		{
			return Bounds;
		}

		const FVector Extent(Amount, Amount, Amount);
		return FBoundingBox(Bounds.Min - Extent, Bounds.Max + Extent);
	}

	const char* BoolText(bool bValue)
	{
		return bValue ? "Y" : "N";
	}

	FVector GetHitNormal(const FHitResult& Hit)
	{
		return !Hit.WorldNormal.IsNearlyZero() ? Hit.WorldNormal : Hit.ImpactNormal;
	}

	FString GetHitActorName(const FHitResult& Hit)
	{
		return Hit.HitActor ? Hit.HitActor->GetName() : FString("None");
	}

	FString GetHitComponentName(const FHitResult& Hit)
	{
		return Hit.HitComponent ? Hit.HitComponent->GetName() : FString("None");
	}

	float HitDistanceOrMiss(bool bHit, const FHitResult& Hit)
	{
		return bHit ? Hit.Distance : -1.0f;
	}

	bool FillStaticMeshTraceHit(
		UStaticMeshComponent* Component,
		const FVector& Start,
		const FVector& Direction,
		FHitResult& Hit)
	{
		if (!Component || !Hit.bHit) return false;

		UStaticMesh* StaticMesh = Component->GetStaticMesh();
		FStaticMesh* Asset = StaticMesh ? StaticMesh->GetStaticMeshAsset() : nullptr;
		if (!Asset || Hit.FaceIndex < 0 || Hit.FaceIndex + 2 >= static_cast<int32>(Asset->Indices.size()))
		{
			return false;
		}

		const uint32 I0 = Asset->Indices[Hit.FaceIndex];
		const uint32 I1 = Asset->Indices[Hit.FaceIndex + 1];
		const uint32 I2 = Asset->Indices[Hit.FaceIndex + 2];
		if (I0 >= Asset->Vertices.size() || I1 >= Asset->Vertices.size() || I2 >= Asset->Vertices.size())
		{
			return false;
		}

		const FVector& V0 = Asset->Vertices[I0].pos;
		const FVector& V1 = Asset->Vertices[I1].pos;
		const FVector& V2 = Asset->Vertices[I2].pos;

		FVector LocalNormal = (V1 - V0).Cross(V2 - V0);
		if (LocalNormal.IsNearlyZero()) return false;
		LocalNormal.Normalize();

		FVector WorldNormal = Component->GetWorldMatrix().TransformVector(LocalNormal);
		if (WorldNormal.IsNearlyZero()) return false;
		WorldNormal.Normalize();

		FVector RayDirection = Direction;
		if (!RayDirection.IsNearlyZero())
		{
			RayDirection.Normalize();
			if (WorldNormal.Dot(RayDirection) > 0.0f)
			{
				WorldNormal = WorldNormal * -1.0f;
			}
		}

		Hit.HitComponent = Component;
		Hit.HitActor = Component->GetOwner();
		Hit.WorldHitLocation = Start + RayDirection * Hit.Distance;
		Hit.WorldNormal = WorldNormal;
		Hit.ImpactNormal = WorldNormal;
		Hit.bHit = true;
		return true;
	}

	FVector ClampPointToBounds(const FVector& Point, const FBoundingBox& Bounds)
	{
		return FVector(
			FMath::Clamp(Point.X, Bounds.Min.X, Bounds.Max.X),
			FMath::Clamp(Point.Y, Bounds.Min.Y, Bounds.Max.Y),
			FMath::Clamp(Point.Z, Bounds.Min.Z, Bounds.Max.Z));
	}

	void ConsiderBoundsFace(
		float Distance,
		const FVector& Normal,
		int32 Axis,
		float FaceValue,
		float& BestDistance,
		FVector& BestNormal,
		int32& BestAxis,
		float& BestFaceValue)
	{
		if (Distance >= BestDistance) return;

		BestDistance = Distance;
		BestNormal = Normal;
		BestAxis = Axis;
		BestFaceValue = FaceValue;
	}

	bool BuildBoundsWallHit(
		UStaticMeshComponent* Component,
		const FBoundingBox& Bounds,
		const FVector& Start,
		const FVector& Direction,
		float MaxDistance,
		FHitResult& OutHit)
	{
		if (!Component || !Bounds.IsValid()) return false;

		FVector RayDirection = Direction;
		if (RayDirection.IsNearlyZero()) return false;
		RayDirection.Normalize();

		FVector HitLocation = ClampPointToBounds(Start, Bounds);
		FVector Normal = Start - HitLocation;
		float Distance = Normal.Length();

		if (Distance <= 1.e-4f)
		{
			float BestFaceDistance = FLT_MAX;
			FVector BestFaceNormal = FVector::ZeroVector;
			int32 BestAxis = 0;
			float BestFaceValue = 0.0f;

			ConsiderBoundsFace(Start.X - Bounds.Min.X, FVector(-1.0f, 0.0f, 0.0f), 0, Bounds.Min.X, BestFaceDistance, BestFaceNormal, BestAxis, BestFaceValue);
			ConsiderBoundsFace(Bounds.Max.X - Start.X, FVector(1.0f, 0.0f, 0.0f), 0, Bounds.Max.X, BestFaceDistance, BestFaceNormal, BestAxis, BestFaceValue);
			ConsiderBoundsFace(Start.Y - Bounds.Min.Y, FVector(0.0f, -1.0f, 0.0f), 1, Bounds.Min.Y, BestFaceDistance, BestFaceNormal, BestAxis, BestFaceValue);
			ConsiderBoundsFace(Bounds.Max.Y - Start.Y, FVector(0.0f, 1.0f, 0.0f), 1, Bounds.Max.Y, BestFaceDistance, BestFaceNormal, BestAxis, BestFaceValue);
			ConsiderBoundsFace(Start.Z - Bounds.Min.Z, FVector(0.0f, 0.0f, -1.0f), 2, Bounds.Min.Z, BestFaceDistance, BestFaceNormal, BestAxis, BestFaceValue);
			ConsiderBoundsFace(Bounds.Max.Z - Start.Z, FVector(0.0f, 0.0f, 1.0f), 2, Bounds.Max.Z, BestFaceDistance, BestFaceNormal, BestAxis, BestFaceValue);

			if (BestFaceNormal.IsNearlyZero() || BestFaceDistance == FLT_MAX) return false;

			HitLocation = Start;
			if (BestAxis == 0)
			{
				HitLocation.X = BestFaceValue;
			}
			else if (BestAxis == 1)
			{
				HitLocation.Y = BestFaceValue;
			}
			else
			{
				HitLocation.Z = BestFaceValue;
			}

			Normal = BestFaceNormal;
			Distance = BestFaceDistance;
		}
		else
		{
			Normal.Normalize();
		}

		if (Distance > MaxDistance) return false;

		const FVector ToHit = HitLocation - Start;
		if (!ToHit.IsNearlyZero() && ToHit.Normalized().Dot(RayDirection) < 0.10f)
		{
			return false;
		}

		if (Normal.Dot(RayDirection) > 0.0f)
		{
			Normal = Normal * -1.0f;
		}

		OutHit = FHitResult{};
		OutHit.HitComponent = Component;
		OutHit.HitActor = Component->GetOwner();
		OutHit.Distance = Distance;
		OutHit.WorldHitLocation = HitLocation;
		OutHit.WorldNormal = Normal;
		OutHit.ImpactNormal = Normal;
		OutHit.bHit = true;
		return true;
	}
}

UCharacterMovementComponent::UCharacterMovementComponent()
{
	// USkeletalMeshComponent::TickComponent (TG_PrePhysics, default) 가 UpdateAnimation 으로
	// AnimInstance->PendingRootMotion 을 채운 다음에 CMC 가 그 값을 가져가야 같은 frame 데이터를
	// 쓸 수 있다. Prerequisite API 가 우리 엔진에 없으므로 TickGroup 분리로 순서 보장.
	// FTickManager 가 group 순서대로 실행하므로 PrePhysics 가 모두 끝난 뒤 DuringPhysics 가 돈다.
	PrimaryComponentTick.SetTickGroup(TG_DuringPhysics);
	PrimaryComponentTick.SetEndTickGroup(TG_DuringPhysics);
}

void UCharacterMovementComponent::EndPlay()
{
	ReleaseController();
	Super::EndPlay();
}

bool UCharacterMovementComponent::EnsureController()
{
	UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(GetUpdatedComponent());
	if (!Capsule) return false;

	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();

	const bool bControllerShapeChanged = ControllerUpdatedComponent != Capsule ||
		std::fabs(CachedControllerRadius - Radius) > FMath::Epsilon ||
		std::fabs(CachedControllerHalfHeight - HalfHeight) > FMath::Epsilon ||
		std::fabs(CachedControllerContactOffset - ControllerContactOffset) > FMath::Epsilon ||
		std::fabs(CachedMaxStepHeight - MaxStepHeight) > FMath::Epsilon ||
		std::fabs(CachedWalkableSlopeAngle - WalkableSlopeAngle) > FMath::Epsilon;

	if (Controller && !bControllerShapeChanged) return true;

	if (Controller)
	{
		ReleaseController();
	}

	UWorld* World = GetWorld();
	if (!World || !World->GetPhysicsScene()) return false;

	physx::PxControllerManager* Manager = World->GetPhysicsScene()->GetControllerManager();
	physx::PxMaterial* Material = FPhysXSDK::Get().GetDefaultMaterial();
	if (!Manager || !Material) return false;

	const FVector Location = Capsule->GetWorldLocation();
	const float CylinderHeight = std::max(0.01f, HalfHeight * 2.0f - Radius * 2.0f);

	physx::PxCapsuleControllerDesc Desc;
	Desc.position = physx::PxExtendedVec3(Location.X, Location.Y, Location.Z);
	Desc.radius = Radius;
	Desc.height = CylinderHeight;
	Desc.upDirection = physx::PxVec3(0.0f, 0.0f, 1.0f);
	Desc.material = Material;
	Desc.contactOffset = ControllerContactOffset;
	Desc.stepOffset = MaxStepHeight;
	Desc.slopeLimit = std::cos(WalkableSlopeAngle * FMath::DegToRad);

	if (!Desc.isValid()) return false;

	Controller = Manager->createController(Desc);

	if (Controller)
	{
		ControllerUpdatedComponent = Capsule;
		CachedControllerRadius = Radius;
		CachedControllerHalfHeight = HalfHeight;
		CachedControllerContactOffset = ControllerContactOffset;
		CachedMaxStepHeight = MaxStepHeight;
		CachedWalkableSlopeAngle = WalkableSlopeAngle;
		return true;
	}

	return false;
}

void UCharacterMovementComponent::ReleaseController()
{
	if (Controller)
	{
		Controller->release();
		Controller = nullptr;
	}

	ControllerUpdatedComponent = nullptr;
	CachedControllerRadius = 0.0f;
	CachedControllerHalfHeight = 0.0f;
	CachedControllerContactOffset = 0.0f;
	CachedMaxStepHeight = 0.0f;
	CachedWalkableSlopeAngle = 0.0f;
}

void UCharacterMovementComponent::SyncUpdatedComponentFromController()
{
	if (!Controller) return;

	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated) return;

	const physx::PxExtendedVec3 Position = Controller->getPosition();
	Updated->SetWorldLocation(FVector(static_cast<float>(Position.x),
		static_cast<float>(Position.y), static_cast<float>(Position.z)));

	if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Updated))
	{
		if (FBodyInstance* Body = Primitive->GetBodyInstance())
		{
			Body->SyncToPhysics();
		}
	}
}

void UCharacterMovementComponent::SyncControllerToUpdatedComponentIfNeeded()
{
	if (!Controller) return;

	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated) return;

	const FVector UpdatedLocation = Updated->GetWorldLocation();

	const physx::PxExtendedVec3 ControllerPosition = Controller->getPosition();
	const FVector ControllerLocation(
		static_cast<float>(ControllerPosition.x),
		static_cast<float>(ControllerPosition.y),
		static_cast<float>(ControllerPosition.z));

	const float MaxDistanceSq = ControllerSyncTeleportDistance * ControllerSyncTeleportDistance;
	if (FVector::DistSquared(UpdatedLocation, ControllerLocation) <= MaxDistanceSq) return;

	Controller->setPosition(physx::PxExtendedVec3(UpdatedLocation.X, UpdatedLocation.Y, UpdatedLocation.Z));

	GroundMissFrames = 0;
}

bool UCharacterMovementComponent::IsWalkableFloorHit(const FHitResult& Hit) const
{
	if (!Hit.bHit)
	{
		return false;
	}

	const FVector Normal = GetHitNormal(Hit);
	if (Normal.IsNearlyZero())
	{
		return false;
	}

	const float MinWalkableUpDot = std::cos(WalkableSlopeAngle * FMath::DegToRad);
	return Normal.Normalized().Dot(FVector::UpVector) >= MinWalkableUpDot;
}

bool UCharacterMovementComponent::FindFloor(FHitResult& OutFloorHit) const
{
	UWorld* World = GetWorld();
	const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(GetUpdatedComponent());
	if (!World || !Capsule)
	{
		OutFloorHit = FHitResult{};
		return false;
	}

	const FVector CapsuleCenter = Capsule->GetWorldLocation();
	const float CapsuleRadius = Capsule->GetScaledCapsuleRadius();
	const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float BottomSphereOffset = std::max(0.0f, CapsuleHalfHeight - CapsuleRadius);
	const FVector BottomSphereCenter = CapsuleCenter - FVector::UpVector * BottomSphereOffset;
	const FVector ProbeStart = BottomSphereCenter + FVector::UpVector * ControllerContactOffset;
	const float ProbeDistance = FloorProbeDistance + ControllerContactOffset + 0.05f;
	const float ProbeRadius = std::max(0.01f, CapsuleRadius * 0.92f);

	FHitResult SweepHit;
	const bool bSweepHit = World->PhysicsSweepSphere(
		ProbeStart,
		FVector(0.0f, 0.0f, -1.0f),
		ProbeDistance,
		ProbeRadius,
		SweepHit,
		ECollisionChannel::Pawn,
		GetOwner());

	if (bSweepHit && IsWalkableFloorHit(SweepHit))
	{
		OutFloorHit = SweepHit;
		return true;
	}

	FHitResult RayHit;
	const bool bRayHit = World->PhysicsRaycast(
		CapsuleCenter,
		FVector(0.0f, 0.0f, -1.0f),
		CapsuleHalfHeight + FloorProbeDistance + ControllerContactOffset + 0.05f,
		RayHit,
		ECollisionChannel::Pawn,
		GetOwner());

	if (bRayHit && IsWalkableFloorHit(RayHit))
	{
		OutFloorHit = RayHit;
		return true;
	}

	if (bSweepHit)
	{
		OutFloorHit = SweepHit;
		return true;
	}
	if (bRayHit)
	{
		OutFloorHit = RayHit;
		return true;
	}

	OutFloorHit = FHitResult{};
	return false;
}

void UCharacterMovementComponent::SetCrouching(bool bEnable)
{
	if (bEnable && bIgnoreCrouchInputUntilRelease)
	{
		bWasCrouchInputDown = true;
		bWantsCrouch = false;
		return;
	}

	if (bEnable && !bWasCrouchInputDown)
	{
		bSlideQueued = true;
		SlideQueueTimer = (std::max)(0.0f, SlideQueueGraceTime);
	}
	else if (!bEnable)
	{
		if (bIsSliding)
		{
			EndSlide(false);
		}
		bSlideQueued = false;
		SlideQueueTimer = 0.0f;
		bIgnoreCrouchInputUntilRelease = false;
	}

	bWasCrouchInputDown = bEnable;

	if (bWantsCrouch == bEnable)
	{
		if (!bEnable)
		{
			StopSlideAudio();
		}
		return;
	}

	bWantsCrouch = bEnable;
	if (!bWantsCrouch)
	{
		StopSlideAudio();
	}
}

void UCharacterMovementComponent::UpdateCrouchState(float DeltaTime)
{
	UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(GetUpdatedComponent());
	if (!Capsule) return;

	const float CurrentHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	const float Radius = Capsule->GetUnscaledCapsuleRadius();

	if (!bIsCrouched)
	{
		StandingCapsuleHalfHeight = (std::max)(CurrentHalfHeight, Radius);
	}

	const float StandingHalfHeight = StandingCapsuleHalfHeight > 0.0f
		? (std::max)(StandingCapsuleHalfHeight, Radius)
		: (std::max)(CurrentHalfHeight, Radius);
	const float TargetCrouchedHalfHeight = FMath::Clamp(CrouchedHalfHeight, Radius, StandingHalfHeight);

	const bool bShouldCrouch = bWantsCrouch || bIsSliding;

	if (bShouldCrouch)
	{
		bIsCrouched = true;
	}
	else if (!bIsCrouched)
	{
		return;
	}
	else if (!CanStandUp(Capsule, StandingHalfHeight))
	{
		return;
	}

	const float TargetHalfHeight = bShouldCrouch ? TargetCrouchedHalfHeight : StandingHalfHeight;
	const float HalfHeightDelta = TargetHalfHeight - CurrentHalfHeight;
	if (std::fabs(HalfHeightDelta) <= FMath::Epsilon)
	{
		if (!bShouldCrouch)
		{
			bIsCrouched = false;
		}
		return;
	}

	const float BlendSpeed = (std::max)(0.0f, CrouchBlendSpeed);
	const float MaxStep = BlendSpeed > FMath::Epsilon ? BlendSpeed * DeltaTime : std::fabs(HalfHeightDelta);
	const float NewHalfHeight = std::fabs(HalfHeightDelta) <= MaxStep
		? TargetHalfHeight
		: CurrentHalfHeight + (HalfHeightDelta > 0.0f ? MaxStep : -MaxStep);

	ApplyCapsuleHalfHeight(Capsule, NewHalfHeight);

	if (!bShouldCrouch && std::fabs(NewHalfHeight - StandingHalfHeight) <= FMath::Epsilon)
	{
		bIsCrouched = false;
	}
}

bool UCharacterMovementComponent::CanStandUp(const UCapsuleComponent* Capsule, float StandingHalfHeight) const
{
	UWorld* World = GetWorld();
	if (!World || !Capsule) return false;

	const float CurrentHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float StandingScaledHalfHeight = StandingHalfHeight * Capsule->GetWorldScale().Z;
	if (StandingScaledHalfHeight <= CurrentHalfHeight + FMath::Epsilon)
	{
		return true;
	}

	const FVector Center = Capsule->GetWorldLocation();
	const float CurrentTopSphereOffset = (std::max)(0.0f, CurrentHalfHeight - Radius);
	const float StandingTopSphereOffset = (std::max)(0.0f, StandingScaledHalfHeight - Radius);
	const float CenterRaise = StandingScaledHalfHeight - CurrentHalfHeight;

	const FVector Start = Center + FVector::UpVector * CurrentTopSphereOffset;
	const FVector End = Center + FVector::UpVector * (CenterRaise + StandingTopSphereOffset);
	const FVector Delta = End - Start;
	const float Distance = Delta.Length();
	if (Distance <= FMath::Epsilon)
	{
		return true;
	}

	FHitResult Hit;
	return !World->PhysicsSweepSphere(
		Start,
		Delta * (1.0f / Distance),
		Distance + ControllerContactOffset,
		Radius,
		Hit,
		ECollisionChannel::Pawn,
		GetOwner());
}

void UCharacterMovementComponent::ApplyCapsuleHalfHeight(UCapsuleComponent* Capsule, float NewHalfHeight)
{
	if (!Capsule) return;

	const float CurrentHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	if (std::fabs(CurrentHalfHeight - NewHalfHeight) <= FMath::Epsilon)
	{
		return;
	}

	const float ScaleZ = Capsule->GetWorldScale().Z;
	const float WorldHalfHeightDelta = (CurrentHalfHeight - NewHalfHeight) * ScaleZ;
	const FVector NewCenter = Capsule->GetWorldLocation() - FVector::UpVector * WorldHalfHeightDelta;

	Capsule->SetCapsuleSize(Capsule->GetUnscaledCapsuleRadius(), NewHalfHeight);
	Capsule->SetWorldLocation(NewCenter);

	if (FBodyInstance* Body = Capsule->GetBodyInstance())
	{
		Body->SyncToPhysics();
	}

	ReleaseController();
}

FControllerMoveResult UCharacterMovementComponent::MoveController(
	const FVector& Delta,
	float DeltaTime)
{
	if (!EnsureController())
	{
		return FControllerMoveResult();
	}

	const physx::PxVec3 Disp(Delta.X, Delta.Y, Delta.Z);
	FPhysicsRaycastFilterCallback QueryFilter(ECollisionChannel::Pawn, GetOwner());

	physx::PxControllerFilters Filters(nullptr, &QueryFilter, nullptr);

	const physx::PxControllerCollisionFlags Flags = Controller->move(Disp, ControllerMinMoveDistance, DeltaTime, Filters);
	FControllerMoveResult Result;
	Result.bHitDown = Flags.isSet(physx::PxControllerCollisionFlag::eCOLLISION_DOWN);

	SyncUpdatedComponentFromController();
	Result.bHasFloorProbeHit = FindFloor(Result.FloorHit);
	Result.bHasWalkableFloor = Result.bHasFloorProbeHit && IsWalkableFloorHit(Result.FloorHit);

	if (Result.bHitDown && !Result.bHasWalkableFloor && bLogWallRunDiagnostics && ShouldEmitWallRunDiagnostics())
	{
		const FVector FloorNormal = Result.bHasFloorProbeHit ? GetHitNormal(Result.FloorHit) : FVector::ZeroVector;
		const float UpDot = FloorNormal.IsNearlyZero() ? 0.0f : FloorNormal.Normalized().Dot(FVector::UpVector);
		const float MinWalkableUpDot = std::cos(WalkableSlopeAngle * FMath::DegToRad);
		UE_LOG(
			"[FloorProbe] rawDown=Y walkable=N probeHit=%s dist=%.2f normal=(%.2f,%.2f,%.2f) upDot=%.2f minUpDot=%.2f actor=%s comp=%s",
			BoolText(Result.bHasFloorProbeHit),
			HitDistanceOrMiss(Result.bHasFloorProbeHit, Result.FloorHit),
			FloorNormal.X,
			FloorNormal.Y,
			FloorNormal.Z,
			UpDot,
			MinWalkableUpDot,
			GetHitActorName(Result.FloorHit).c_str(),
			GetHitComponentName(Result.FloorHit).c_str());
	}

	return Result;
}

void UCharacterMovementComponent::AddInputVector(const FVector& WorldDirection, float ScaleValue)
{
	AccumulatedInput = AccumulatedInput + WorldDirection * ScaleValue;
}

void UCharacterMovementComponent::ConsumeInputVector(FVector& Out)
{
	Out = AccumulatedInput;
	AccumulatedInput = FVector(0.0f, 0.0f, 0.0f);
}

void UCharacterMovementComponent::AddRootMotionDelta(const FTransform& LocalDelta)
{
	if (!bHasPendingRootMotion)
	{
		PendingRootMotion = LocalDelta;
		bHasPendingRootMotion = true;
		return;
	}

	// 누적 합성 — AnimInstance::AccumulateRootMotion 과 동일한 매트릭스 곱 패턴.
	// 같은 frame 에 base + montage 처럼 여러 소스가 push 할 수 있어 합성 보장 필요.
	const FMatrix M = LocalDelta.ToMatrix() * PendingRootMotion.ToMatrix();
	PendingRootMotion.Location = FVector(M.M[3][0], M.M[3][1], M.M[3][2]);
	PendingRootMotion.Rotation = (LocalDelta.Rotation * PendingRootMotion.Rotation).GetNormalized();
	// Scale 은 root motion 에서 보통 1 — 무시.
}

bool UCharacterMovementComponent::ConsumePendingRootMotion(FTransform& OutLocalDelta)
{
	if (!bHasPendingRootMotion)
	{
		OutLocalDelta = FTransform();   // Identity
		return false;
	}
	OutLocalDelta = PendingRootMotion;
	PendingRootMotion = FTransform();
	bHasPendingRootMotion = false;
	return true;
}

void UCharacterMovementComponent::SetMovementMode(EMovementMode NewMode)
{
	if (MovementMode == NewMode) return;
	if (bIsSliding && NewMode != EMovementMode::Walking)
	{
		EndSlide(false);
	}
	if (MovementMode == EMovementMode::WallRunning && NewMode != EMovementMode::WallRunning)
	{
		if (bEnableBuiltInMovementAudio)
		{
			FAudioManager::Get().SetLoopState("PlayerWallRunRub", "WallRunRub", false);
		}
		WallRunStepDistance = 0.0f;
	}
	if (NewMode != EMovementMode::Walking)
	{
		StopSlideAudio();
	}
	MovementMode = NewMode;
	if (MovementMode != EMovementMode::Walking)
	{
		ResetSprintFootstepAudio();
	}
	// 추후 OnMovementModeChanged delegate 위치.
}

void UCharacterMovementComponent::ResetSprintFootstepAudio()
{
	SprintFootstepDistance = 0.0f;
}

void UCharacterMovementComponent::UpdateSprintFootstepAudio(float DeltaTime)
{
	if (DeltaTime <= 0.0f || MovementMode != EMovementMode::Walking || IsCrouching())
	{
		ResetSprintFootstepAudio();
		return;
	}

	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated)
	{
		ResetSprintFootstepAudio();
		return;
	}

	const FVector PlanarVelocity(Velocity.X, Velocity.Y, 0.0f);
	const float PlanarSpeed = PlanarVelocity.Length();
	if (PlanarSpeed < SprintFootstepMinSpeed)
	{
		ResetSprintFootstepAudio();
		return;
	}

	const bool bSprint = IsSprinting();
	const float StrideDistance = bSprint ? SprintFootstepStrideDistance : WalkFootstepStrideDistance;
	if (SprintFootstepDistance <= 0.0f)
	{
		SprintFootstepDistance = StrideDistance * SprintFootstepInitialDistanceRatio;
	}

	SprintFootstepDistance += PlanarSpeed * DeltaTime;
	if (SprintFootstepDistance < StrideDistance)
	{
		return;
	}

	SprintFootstepDistance = std::fmod(SprintFootstepDistance, StrideDistance);
	if (bEnableBuiltInMovementAudio)
	{
		FAudioManager::Get().PlayOneShotAt(bSprint ? "player.run.step" : "player.walk.step", Updated->GetWorldLocation());
	}
}

void UCharacterMovementComponent::UpdateSlideAudio(float DeltaTime)
{
	if (DeltaTime <= 0.0f || MovementMode != EMovementMode::Walking || !IsCrouching())
	{
		StopSlideAudio();
		return;
	}

	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated)
	{
		StopSlideAudio();
		return;
	}

	const FVector PlanarVelocity(Velocity.X, Velocity.Y, 0.0f);
	const float PlanarSpeed = PlanarVelocity.Length();
	if (PlanarSpeed < SlideAudioMinSpeed)
	{
		StopSlideAudio();
		return;
	}

	if (!bSlideAudioActive)
	{
		bSlideAudioActive = true;
		SlideStepDistance = 0.0f;
		if (bEnableBuiltInMovementAudio)
		{
			FAudioManager::Get().PlayOneShotAt("player.slide.start", Updated->GetWorldLocation());
		}
	}

	if (bEnableBuiltInMovementAudio)
	{
		FAudioManager::Get().SetLoopState("PlayerSlideLoop", "SlideLoop", true, 0.50f, 1.0f);
	}

	SlideStepDistance += PlanarSpeed * DeltaTime;
	if (SlideStepDistance >= SlideStepStrideDistance)
	{
		SlideStepDistance = std::fmod(SlideStepDistance, SlideStepStrideDistance);
		if (bEnableBuiltInMovementAudio)
		{
			FAudioManager::Get().PlayOneShotAt("player.slide.rub", Updated->GetWorldLocation());
		}
	}
}

void UCharacterMovementComponent::StopSlideAudio()
{
	if (!bSlideAudioActive)
	{
		return;
	}

	bSlideAudioActive = false;
	SlideStepDistance = 0.0f;
	if (bEnableBuiltInMovementAudio)
	{
		FAudioManager::Get().SetLoopState("PlayerSlideLoop", "SlideLoop", false);
		FAudioManager::Get().PlayOneShot("player.slide.end");
	}
}

void UCharacterMovementComponent::PlayLandingAudio(float DownSpeed)
{
	if (!bEnableBuiltInMovementAudio)
	{
		return;
	}

	FAudioManager::Get().StopEvent("player.jump.jet");
	if (DownSpeed >= HeavyLandDownSpeed)
	{
		FAudioManager::Get().PlayOneShot("player.land.heavy");
	}
	else
	{
		FAudioManager::Get().PlayOneShot("player.land");
	}
}

const char* UCharacterMovementComponent::GetMovementModeName() const
{
	switch (MovementMode)
	{
	case EMovementMode::Walking:
		return "Walking";
	case EMovementMode::Falling:
		return "Falling";
	case EMovementMode::WallRunning:
		return "WallRunning";
	default:
		return "";
	}
}

const char* UCharacterMovementComponent::GetWallRunStatusName(EWallRunStatus Status) const
{
	switch (Status)
	{
	case EWallRunStatus::NotFalling:
		return "NOT_FALLING";
	case EWallRunStatus::Disabled:
		return "DISABLED";
	case EWallRunStatus::NoUpdatedComponent:
		return "NO_UPDATED_COMPONENT";
	case EWallRunStatus::NoController:
		return "NO_CONTROLLER";
	case EWallRunStatus::Fatigued:
		return "FATIGUED";
	case EWallRunStatus::NoWall:
		return "NO_WALL";
	case EWallRunStatus::LowSpeed:
		return "LOW_SPEED";
	case EWallRunStatus::BadNormal:
		return "BAD_NORMAL";
	case EWallRunStatus::BadDirection:
		return "BAD_DIRECTION";
	case EWallRunStatus::Active:
		return "ACTIVE";
	case EWallRunStatus::EndedTimeLimit:
		return "ENDED_TIME_LIMIT";
	case EWallRunStatus::EndedNoWall:
		return "ENDED_NO_WALL";
	case EWallRunStatus::EndedBadNormal:
		return "ENDED_BAD_NORMAL";
	case EWallRunStatus::EndedBadDirection:
		return "ENDED_BAD_DIRECTION";
	case EWallRunStatus::Landed:
		return "LANDED";
	default:
		return "UNKNOWN";
	}
}

bool UCharacterMovementComponent::ShouldEmitWallRunDiagnostics() const
{
	if (!bLogWallRunDiagnostics)
	{
		return false;
	}

	const int32 Interval = std::max(1, WallRunDiagnosticsInterval);
	return (WallRunDiagnosticsFrameCounter % Interval) == 0;
}

void UCharacterMovementComponent::SetWallRunStatus(EWallRunStatus NewStatus, const FHitResult* Hit)
{
	const bool bStatusChanged = LastWallRunStatus != NewStatus;
	LastWallRunStatus = NewStatus;

	if (Hit && Hit->bHit)
	{
		LastWallRunStatusHit = *Hit;
		bLastWallRunStatusHasHit = true;
	}
	else
	{
		LastWallRunStatusHit = FHitResult{};
		bLastWallRunStatusHasHit = false;
	}

	if (bLogWallRunDiagnostics && (bStatusChanged || ShouldEmitWallRunDiagnostics()))
	{
		LogWallRunStatus(NewStatus, Hit);
	}
}

void UCharacterMovementComponent::LogWallRunStatus(EWallRunStatus Status, const FHitResult* Hit) const
{
	const FVector PlanarVelocity(Velocity.X, Velocity.Y, 0.0f);
	const float PlanarSpeed = PlanarVelocity.Length();
	const FHitResult EmptyHit;
	const FHitResult& LogHit = Hit ? *Hit : EmptyHit;
	const bool bHasHit = Hit && Hit->bHit;
	const FVector Normal = bHasHit ? GetHitNormal(LogHit) : FVector::ZeroVector;
	const float UpDot = Normal.IsNearlyZero() ? 0.0f : Normal.Normalized().Dot(FVector::UpVector);
	const FString ActorName = GetHitActorName(LogHit);
	const FString ComponentName = GetHitComponentName(LogHit);

	UE_LOG(
		"[WallRunStatus] status=%s mode=%s speed=%.2f planar=%.2f minStart=%.2f check=%.2f radius=%.2f hit=%s dist=%.2f normal=(%.2f,%.2f,%.2f) upDot=%.2f actor=%s comp=%s",
		GetWallRunStatusName(Status),
		GetMovementModeName(),
		Velocity.Length(),
		PlanarSpeed,
		MinWallRunStartSpeed,
		WallCheckDistance,
		WallCheckSphereRadius,
		BoolText(bHasHit),
		HitDistanceOrMiss(bHasHit, LogHit),
		Normal.X,
		Normal.Y,
		Normal.Z,
		UpDot,
		ActorName.c_str(),
		ComponentName.c_str());
}

FWallRunDebugSnapshot UCharacterMovementComponent::GetWallRunDebugSnapshot() const
{
	FWallRunDebugSnapshot Snapshot;
	const FVector PlanarVelocity(Velocity.X, Velocity.Y, 0.0f);
	const bool bHasHit = bLastWallRunStatusHasHit && LastWallRunStatusHit.bHit;
	const FVector Normal = bHasHit ? GetHitNormal(LastWallRunStatusHit) : FVector::ZeroVector;
	const float UpDot = Normal.IsNearlyZero() ? 0.0f : Normal.Normalized().Dot(FVector::UpVector);

	Snapshot.MovementModeName = GetMovementModeName();
	Snapshot.StatusName = GetWallRunStatusName(LastWallRunStatus);
	Snapshot.Velocity = Velocity;
	Snapshot.WallNormal = WallRunNormal;
	Snapshot.WallDirection = WallRunDirection;
	Snapshot.HitNormal = Normal;
	Snapshot.Speed = Velocity.Length();
	Snapshot.PlanarSpeed = PlanarVelocity.Length();
	Snapshot.AlongWallSpeed = bHasHit ? GetWallRunAlongSpeed(Normal) : 0.0f;
	Snapshot.MinStartSpeed = MinWallRunStartSpeed;
	Snapshot.WallCheckDistance = WallCheckDistance;
	Snapshot.WallCheckSphereRadius = WallCheckSphereRadius;
	Snapshot.WallRunElapsedTime = WallRunElapsedTime;
	Snapshot.MaxWallRunTime = MaxWallRunTime;
	Snapshot.HitDistance = HitDistanceOrMiss(bHasHit, LastWallRunStatusHit);
	Snapshot.HitUpDot = UpDot;
	Snapshot.bWallRunEnabled = bEnableWallRun;
	Snapshot.bIsWallRunning = MovementMode == EMovementMode::WallRunning;
	Snapshot.bHasHit = bHasHit;
	Snapshot.bOnRightSide = bWallRunOnRightSide;
	Snapshot.bDrawDistanceDebug = bDrawWallRunDistanceDebug;
	Snapshot.bLogDiagnostics = bLogWallRunDiagnostics;
	Snapshot.bLegacyScreenText = bShowWallRunStatusText;
	Snapshot.HitActorName = GetHitActorName(LastWallRunStatusHit);
	Snapshot.HitComponentName = GetHitComponentName(LastWallRunStatusHit);
	return Snapshot;
}

void UCharacterMovementComponent::DrawWallRunStatusText() const
{
	if (!bShowWallRunStatusText)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FWallRunDebugSnapshot Snapshot = GetWallRunDebugSnapshot();

	char Line0[160];
	std::snprintf(Line0, sizeof(Line0), "WALLRUN: %s", Snapshot.StatusName);

	char Line1[256];
	std::snprintf(
		Line1,
		sizeof(Line1),
		"mode=%s speed=%.2f along=%.2f/%.2f check=%.2f radius=%.2f side=%s",
		Snapshot.MovementModeName,
		Snapshot.PlanarSpeed,
		Snapshot.AlongWallSpeed,
		Snapshot.MinStartSpeed,
		Snapshot.WallCheckDistance,
		Snapshot.WallCheckSphereRadius,
		Snapshot.bOnRightSide ? "R" : "L");

	char Line2[256];
	std::snprintf(
		Line2,
		sizeof(Line2),
		"hit=%s dist=%.2f upDot=%.2f actor=%s",
		BoolText(Snapshot.bHasHit),
		Snapshot.HitDistance,
		Snapshot.HitUpDot,
		Snapshot.HitActorName.c_str());

	FScene& Scene = World->GetScene();
	Scene.AddOverlayText(FString(Line0), FVector2(24.0f, 78.0f), 0.90f);
	Scene.AddOverlayText(FString(Line1), FVector2(24.0f, 102.0f), 0.62f);
	Scene.AddOverlayText(FString(Line2), FVector2(24.0f, 120.0f), 0.62f);
}

float UCharacterMovementComponent::GetWallRunCapsuleRadius() const
{
	const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(GetUpdatedComponent());
	return Capsule ? Capsule->GetScaledCapsuleRadius() : 0.0f;
}

FVector UCharacterMovementComponent::GetWallRunSweepStart(const FVector& Direction) const
{
	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated)
	{
		return FVector::ZeroVector;
	}

	FVector SideDirection(Direction.X, Direction.Y, 0.0f);
	if (SideDirection.IsNearlyZero())
	{
		return Updated->GetWorldLocation();
	}
	SideDirection.Normalize();

	return Updated->GetWorldLocation() + SideDirection * GetWallRunCapsuleRadius();
}

float UCharacterMovementComponent::GetWallRunAlongSpeed(const FVector& WallNormal) const
{
	if (WallNormal.IsNearlyZero())
	{
		return 0.0f;
	}

	const FVector RunDirection = ComputeWallRunDirection(WallNormal);
	if (RunDirection.IsNearlyZero())
	{
		return 0.0f;
	}

	const FVector PlanarVelocity(Velocity.X, Velocity.Y, 0.0f);
	if (PlanarVelocity.IsNearlyZero())
	{
		return 0.0f;
	}

	return std::fabs(PlanarVelocity.Dot(RunDirection));
}

// ─── View tilt (TitanFall-style) ──────────────────────────────────────────────
// Mode 별 의도:
//   WallRunning      → Sign = ±1 (벽 측면 기준), Intensity = 1.
//   Falling          → 후보 벽이 (CapsuleRadius + WallCheckDistance * Scale) 안에 있고
//                      reattach cooldown 안 걸려 있으면 Anticipation 부분 강도.
//   Walking / 기타   → 0.
// FindWallRunSurface 는 const + 부수효과 없음 → Falling 중에만 1회 추가 sweep 비용.
void UCharacterMovementComponent::UpdateWallTiltTarget()
{
	if (!bEnableWallRunCameraTilt)
	{
		TargetTiltSign = 0.0f;
		TargetTiltIntensity = 0.0f;
		return;
	}

	if (MovementMode == EMovementMode::WallRunning)
	{
		// 엔진 Roll 컨벤션: +Roll = view 상단이 오른쪽 (시계방향).
		// "벽 반대쪽으로 기울이기" → 벽 오른쪽이면 -Roll (상단 왼쪽으로), 벽 왼쪽이면 +Roll.
		const float RawSign = bWallRunOnRightSide ? -1.0f : +1.0f;
		TargetTiltSign = bInvertWallRunCameraTiltSign ? -RawSign : RawSign;
		TargetTiltIntensity = 1.0f;
		return;
	}

	if (MovementMode == EMovementMode::Falling)
	{
		// Wall-jump 직후엔 같은 벽으로 미리 기울이지 않음 — TryStartWallRun 의 게이트와 같은 의도.
		if (WallJumpReattachTimer > 0.0f)
		{
			TargetTiltSign = 0.0f;
			TargetTiltIntensity = 0.0f;
			return;
		}

		FHitResult ProbeHit;
		bool bRightSide = false;
		if (!FindWallRunSurface(ProbeHit, bRightSide))
		{
			TargetTiltSign = 0.0f;
			TargetTiltIntensity = 0.0f;
			return;
		}

		// 거리 게이트 — WallCheckDistance 자체를 넘어서면 hit 도 없음 (FindWallRun 안에서 컷).
		// 여기선 anticipation 의 거리 → intensity 매핑만:
		//   d ≤ WallCheckDistance               → Intensity = AnticipationIntensity (full anticipation).
		//   d ∈ (WallCheckDistance, MaxDist]    → 0 으로 선형 감쇠.
		const float CapsuleRadius = GetWallRunCapsuleRadius();
		const float NearDist = CapsuleRadius + WallCheckDistance;
		const float FarDist  = CapsuleRadius + WallCheckDistance * std::max(1.0f, WallTiltAnticipationDistanceScale);
		const float D = ProbeHit.Distance;

		float Intensity = 0.0f;
		if (D <= NearDist)
		{
			Intensity = WallTiltAnticipationIntensity;
		}
		else if (D < FarDist && FarDist > NearDist)
		{
			const float T = 1.0f - (D - NearDist) / (FarDist - NearDist);
			Intensity = WallTiltAnticipationIntensity * std::clamp(T, 0.0f, 1.0f);
		}

		// Active 와 동일 부호 규약 — 벽 오른쪽이면 -Roll, 왼쪽이면 +Roll.
		const float RawSign = bRightSide ? -1.0f : +1.0f;
		TargetTiltSign = bInvertWallRunCameraTiltSign ? -RawSign : RawSign;
		TargetTiltIntensity = Intensity;
		return;
	}

	TargetTiltSign = 0.0f;
	TargetTiltIntensity = 0.0f;
}

float UCharacterMovementComponent::GetDesiredCameraRollDeg() const
{
	if (!bEnableWallRunCameraTilt) return 0.0f;
	return TargetTiltSign * TargetTiltIntensity * MaxWallRunCameraTiltDeg;
}

float UCharacterMovementComponent::GetCameraTiltResponseHz(bool bIsIncreasing) const
{
	return bIsIncreasing ? WallTiltAttachResponseHz : WallTiltReleaseResponseHz;
}

float UCharacterMovementComponent::GetWallRunInputAlong(const FVector& Input, const FVector& RunDirection) const
{
	FVector PlanarInput(Input.X, Input.Y, 0.0f);
	FVector PlanarRunDirection(RunDirection.X, RunDirection.Y, 0.0f);
	if (PlanarInput.IsNearlyZero() || PlanarRunDirection.IsNearlyZero())
	{
		return 0.0f;
	}

	PlanarInput.Normalize();
	PlanarRunDirection.Normalize();
	return PlanarInput.Dot(PlanarRunDirection);
}

void UCharacterMovementComponent::DrawWallRunDistanceDebug() const
{
	if (!bDrawWallRunDistanceDebug)
	{
		return;
	}

	UWorld* World = GetWorld();
	USceneComponent* Updated = GetUpdatedComponent();
	if (!World || !Updated)
	{
		return;
	}

	const FVector Center = Updated->GetWorldLocation();
	const float CapsuleRadius = GetWallRunCapsuleRadius();
	const float CheckRadius = CapsuleRadius + std::max(0.0f, WallCheckDistance);
	constexpr int32 SegmentCount = 48;
	const float AngleStep = 2.0f * 3.14159265f / static_cast<float>(SegmentCount);

	auto DrawCircle = [&](float Radius, const FColor& Color)
	{
		if (Radius <= 0.0f)
		{
			return;
		}

		for (int32 Index = 0; Index < SegmentCount; ++Index)
		{
			const float A0 = AngleStep * static_cast<float>(Index);
			const float A1 = AngleStep * static_cast<float>(Index + 1);
			const FVector P0 = Center + FVector(std::cos(A0) * Radius, std::sin(A0) * Radius, 0.0f);
			const FVector P1 = Center + FVector(std::cos(A1) * Radius, std::sin(A1) * Radius, 0.0f);
			World->GetScene().AddDebugLine(P0, P1, Color);
		}
	};

	DrawCircle(CapsuleRadius, FColor(80, 180, 255));
	DrawCircle(CheckRadius, FColor::Yellow());
}

void UCharacterMovementComponent::Jump()
{
	// Walking: 항상 1회. Falling: JumpsRemaining 이 남아 있을 때만 (더블/멀티 점프).
	// WallRunning: 항상 1회 — TickWallRunning 가 wall-jump 임펄스로 변환해 소비.
	// edge-triggered — bWantsJump 만 set, 실제 적용은 Tick 분기에서 consume.
	if (bIsSliding)
	{
		EndSlide(false);
	}

	if (MovementMode == EMovementMode::Walking)
	{
		bWantsJump = true;
	}
	else if (MovementMode == EMovementMode::Falling && JumpsRemaining > 0)
	{
		if (FatiguedAirJumpInputLockTimer > 0.0f)
		{
			return;
		}
		bWantsJump = true;
	}
	else if (MovementMode == EMovementMode::WallRunning)
	{
		bWantsJump = true;
	}
}

void UCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated)
	{
		SetWallRunStatus(EWallRunStatus::NoUpdatedComponent);
		DrawWallRunStatusText();
		DrawWallRunDistanceDebug();
		return;
	}
	if (DeltaTime <= 0.0f) return;

	++WallRunDiagnosticsFrameCounter;
	UpdateCrouchState(DeltaTime);

	// Wall-jump 재진입 쿨다운 타이머 감쇠. 0 이하가 되면 TryStartWallRun 의 게이트는 자동 통과.
	if (WallJumpReattachTimer > 0.0f)
	{
		WallJumpReattachTimer = std::max(0.0f, WallJumpReattachTimer - DeltaTime);
		if (WallJumpReattachTimer <= 0.0f)
		{
			LastWallJumpNormal = FVector::ZeroVector;
		}
	}
	if (WallRunFatigueTimer > 0.0f)
	{
		WallRunFatigueTimer = std::max(0.0f, WallRunFatigueTimer - DeltaTime);
	}
	if (FatiguedAirJumpInputLockTimer > 0.0f)
	{
		FatiguedAirJumpInputLockTimer = std::max(0.0f, FatiguedAirJumpInputLockTimer - DeltaTime);
	}

	if (!EnsureController())
	{
		SetWallRunStatus(EWallRunStatus::NoController);
		DrawWallRunStatusText();
		DrawWallRunDistanceDebug();
		return;
	}

	SyncControllerToUpdatedComponentIfNeeded();

	// 매 Tick 회전 적용 상태 reset — 이번 frame 에 root motion 이 yaw 를 적용했는지를
	// 외부 (Character::Tick) 가 query 할 수 있어야 yaw 충돌 회피 가능.
	bAppliedRootMotionYawThisFrame = false;
	bAirJumpConsumedThisFrame = false;

	FVector Input;
	ConsumeInputVector(Input);
	Input.Z = 0.0f;   // XY 평면만 — Z 는 mode 가 결정.

	// 1) Input 처리 — XY velocity 갱신 (양 mode 공통).
	if (bSlideQueued)
	{
		if (!TryStartSlide(Input))
		{
			SlideQueueTimer -= DeltaTime;
			if (SlideQueueTimer <= 0.0f)
			{
				bSlideQueued = false;
				SlideQueueTimer = 0.0f;
			}
		}
	}

	if (bIsSliding)
	{
		UpdateSlideVelocity(DeltaTime);
	}
	else
	{
		ApplyInputToVelocity(Input, DeltaTime);
	}

	// 1.5) Owner Character 의 Mesh AnimInstance 가 누적해둔 root motion 을 가져와 자기 buffer 로 push.
	//      Mesh tick (TG_PrePhysics) 이 이미 끝나 PendingRootMotion 이 채워진 상태.
	//      Mode 가 Ignore 면 가져갈 필요 자체가 없음 (AccumulateRootMotion 측에서 누적도 안 됨).
	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh())
		{
			if (UAnimInstance* AI = Mesh->GetAnimInstance())
			{
				if (AI->GetRootMotionMode() != ERootMotionMode::IgnoreRootMotion)
				{
					AddRootMotionDelta(AI->ConsumeRootMotion());
				}
			}
		}
	}

	// 2) Root motion 소비 — local delta 를 world frame 으로 변환 (Updated 의 yaw 기준).
	//    XY 만 mode 분기로 위임. Z 는 두 mode 모두 무시:
	//      Walking — floor stick 이 Z 결정
	//      Falling — gravity 가 Z 결정
	//    Climbing/Swimming 같은 mode 추가 시 그때 재검토.
	FTransform RootMotionDelta;
	const bool bHadRootMotion = ConsumePendingRootMotion(RootMotionDelta);
	FVector RootMotionWorldXY(0.0f, 0.0f, 0.0f);
	if (bHadRootMotion)
	{
		const FRotator ActorRot = Updated->GetWorldRotation();
		const FQuat    YawOnly  = FRotator(0.0f, 0.0f, ActorRot.Yaw).ToQuaternion();
		const FVector  World    = YawOnly.RotateVector(RootMotionDelta.Location);
		RootMotionWorldXY.X     = World.X;
		RootMotionWorldXY.Y     = World.Y;
	}

	// 3) Falling 중 벽 후보가 있으면 WallRunning 으로 먼저 전환.
	//    이 검사를 Falling 이동보다 앞에 두면 벽에 닿은 frame 부터 바로 벽타기 이동식이 적용된다.
	if (MovementMode == EMovementMode::Falling)
	{
		TryStartWallRun(Input);
	}

	// 4) Mode 별 Z 처리 + 위치 적용 (input velocity + root motion XY 합산).
	if (MovementMode == EMovementMode::Walking)
	{
		TickWalking(DeltaTime, RootMotionWorldXY);
	}
	else if (MovementMode == EMovementMode::WallRunning)
	{
		TickWallRunning(DeltaTime, RootMotionWorldXY, Input);
	}
	else
	{
		TickFalling(DeltaTime, RootMotionWorldXY);
	}

	UpdateSprintFootstepAudio(DeltaTime);
	UpdateSlideAudio(DeltaTime);

	// 5) Root motion yaw 적용. yaw 만 추출 — root motion 의 pitch/roll 은 캐릭터 capsule
	//    회전에 일반적으로 의미 없음 (UE 도 yaw 만 적용).
	//    yaw 가 적용되면 bAppliedRootMotionYawThisFrame 을 켜서 PhysOrientToMovement /
	//    Character 의 control yaw 덮어쓰기 둘 다 같은 frame skip 되도록 한다.
	if (bHadRootMotion)
	{
		const FRotator DeltaRot = RootMotionDelta.Rotation.ToRotator();
		if (std::fabs(DeltaRot.Yaw) > 1e-4f)
		{
			FRotator R = Updated->GetRelativeRotation();
			R.Yaw += DeltaRot.Yaw;
			Updated->SetRelativeRotation(R);
			bAppliedRootMotionYawThisFrame = true;
		}
	}

	// 6) Orient yaw to movement direction. Root motion 이 yaw 를 잡고 있는 frame 은 skip —
	//    그렇지 않으면 PhysOrient 가 root motion 회전을 Velocity 방향으로 다시 lerp 해
	//    의도된 회전이 무효화된다 (turn-in-place anim 가장 큰 피해).
	if (bOrientRotationToMovement && !bAppliedRootMotionYawThisFrame)
	{
		PhysOrientToMovement(DeltaTime);
	}

	if (MovementMode == EMovementMode::Walking)
	{
		SetWallRunStatus(EWallRunStatus::NotFalling);
	}

	// View tilt target — mode 가 확정된 *후* 갱신해야 Falling/WallRunning 분기가 같은 frame 에 일치.
	UpdateWallTiltTarget();

	DrawWallRunStatusText();
	DrawWallRunDistanceDebug();
}

void UCharacterMovementComponent::PhysOrientToMovement(float DeltaTime)
{
	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated) return;

	// 평면 속도 작으면 회전 skip — 마지막 facing 유지.
	const float SpeedSq2D = Velocity.X * Velocity.X + Velocity.Y * Velocity.Y;
	constexpr float MinSpeedSq = 1e-4f;
	if (SpeedSq2D < MinSpeedSq) return;

	// Target yaw — Velocity 방향. UE 의 atan2(Y, X) 는 +X 가 0°, +Y 가 90° (좌표계 가정).
	const float TargetYaw = std::atan2(Velocity.Y, Velocity.X) * (180.0f / 3.14159265f);

	FRotator R = Updated->GetRelativeRotation();
	const float CurrentYaw = R.Yaw;

	// 최단 회전 방향 (delta ∈ [-180, 180])
	float Delta = TargetYaw - CurrentYaw;
	while (Delta >  180.0f) Delta -= 360.0f;
	while (Delta < -180.0f) Delta += 360.0f;

	const float Step = RotationYawRate * DeltaTime;
	if (std::fabs(Delta) <= Step)
	{
		R.Yaw = TargetYaw;
	}
	else
	{
		R.Yaw = CurrentYaw + (Delta > 0.0f ? Step : -Step);
	}
	Updated->SetRelativeRotation(R);
}

float UCharacterMovementComponent::GetPlanarSpeed() const
{
	const FVector PlanarVelocity(Velocity.X, Velocity.Y, 0.0f);
	return PlanarVelocity.Length();
}

FVector UCharacterMovementComponent::ComputeSlideDirection(const FVector& Input) const
{
	FVector PlanarInput(Input.X, Input.Y, 0.0f);
	if (!PlanarInput.IsNearlyZero())
	{
		PlanarInput.Normalize();
		return PlanarInput;
	}

	FVector PlanarVelocity(Velocity.X, Velocity.Y, 0.0f);
	if (!PlanarVelocity.IsNearlyZero())
	{
		PlanarVelocity.Normalize();
		return PlanarVelocity;
	}

	if (USceneComponent* Updated = GetUpdatedComponent())
	{
		FVector Forward = Updated->GetForwardVector();
		Forward.Z = 0.0f;
		if (!Forward.IsNearlyZero())
		{
			Forward.Normalize();
			return Forward;
		}
	}

	return FVector::ZeroVector;
}

bool UCharacterMovementComponent::TryStartSlide(const FVector& Input)
{
	if (!bEnableSlide || bIsSliding || MovementMode != EMovementMode::Walking)
	{
		return false;
	}

	if (!bWantsSprint || GetPlanarSpeed() < SlideMinStartSpeed)
	{
		return false;
	}

	const FVector Direction = ComputeSlideDirection(Input);
	if (Direction.IsNearlyZero())
	{
		return false;
	}

	StartSlide(Direction);
	return true;
}

void UCharacterMovementComponent::StartSlide(const FVector& InSlideDirection)
{
	FVector Direction(InSlideDirection.X, InSlideDirection.Y, 0.0f);
	if (Direction.IsNearlyZero())
	{
		return;
	}
	Direction.Normalize();

	bIsSliding = true;
	bSlideQueued = false;
	SlideQueueTimer = 0.0f;
	SlideElapsedTime = 0.0f;
	SlideDirection = Direction;

	bool bApplyImpulse = true;
	FHitResult FloorHit;
	if (FindFloor(FloorHit) && FloorHit.bHit)
	{
		const FVector FloorNormal = GetHitNormal(FloorHit);
		if (!FloorNormal.IsNearlyZero() && Direction.Dot(FloorNormal.Normalized()) < -0.05f)
		{
			bApplyImpulse = false;
		}
	}

	if (bApplyImpulse)
	{
		Velocity.X += Direction.X * SlideImpulseSpeed;
		Velocity.Y += Direction.Y * SlideImpulseSpeed;
	}
}

void UCharacterMovementComponent::UpdateSlideVelocity(float DeltaTime)
{
	if (!bIsSliding)
	{
		return;
	}

	if (MovementMode != EMovementMode::Walking)
	{
		EndSlide(false);
		return;
	}

	SlideElapsedTime += DeltaTime;
	if (MaxSlideTime > FMath::Epsilon && SlideElapsedTime >= MaxSlideTime)
	{
		EndSlide(false, true);
		return;
	}

	const float Speed = GetPlanarSpeed();
	if (Speed <= SlideEndSpeed)
	{
		EndSlide(false, true);
		return;
	}

	const float Deceleration = (std::max)(0.0f, SlideBrakingFriction);
	const float NewSpeed = (std::max)(0.0f, Speed - Deceleration * DeltaTime);
	const FVector Direction = FVector(Velocity.X, Velocity.Y, 0.0f) * (1.0f / Speed);

	Velocity.X = Direction.X * NewSpeed;
	Velocity.Y = Direction.Y * NewSpeed;

	if (NewSpeed <= SlideEndSpeed)
	{
		EndSlide(false, true);
	}
}

void UCharacterMovementComponent::EndSlide(bool bKeepCrouchInput, bool bIgnoreHeldCrouchUntilRelease)
{
	if (!bIsSliding)
	{
		return;
	}

	bIsSliding = false;
	bSlideQueued = false;
	SlideQueueTimer = 0.0f;
	SlideElapsedTime = 0.0f;
	SlideDirection = FVector::ZeroVector;

	if (bIgnoreHeldCrouchUntilRelease)
	{
		bWantsCrouch = false;
		bIgnoreCrouchInputUntilRelease = bWasCrouchInputDown;
	}
	else if (bKeepCrouchInput)
	{
		bWantsCrouch = true;
	}
}

void UCharacterMovementComponent::ApplyInputToVelocity(const FVector& Input, float DeltaTime)
{
	if (bIsSliding)
	{
		return;
	}

	if (MovementMode == EMovementMode::WallRunning)
	{
		// WallRunning input은 벽 접선 방향으로 투영해서 TickWallRunning 에서만 처리한다.
		return;
	}

	const float InputLen = Input.Length();
	if (InputLen > 0.0f)
	{
		// 입력 방향으로 가속 (XY 만). Falling 은 기존보다 방향 전환이 약해지도록 70%만 적용.
		const FVector Direction = Input * (1.0f / InputLen);
		constexpr float AirControlScale = 0.7f;
		const float AccelScale = (MovementMode == EMovementMode::Falling) ? AirControlScale : 1.0f;
		Velocity.X += Direction.X * MaxAcceleration * AccelScale * DeltaTime;
		Velocity.Y += Direction.Y * MaxAcceleration * AccelScale * DeltaTime;

		if (MovementMode == EMovementMode::Walking)
		{
			// 지상 입력 중에는 입력 방향 성분은 보존하고, 반대/횡방향 성분만 빨리 죽인다.
			// 전역 damping 이나 물리 설정은 건드리지 않아 다른 모드 영향이 작다.
			FVector V2D(Velocity.X, Velocity.Y, 0.0f);
			const float AlongSpeed = V2D.Dot(Direction);
			const FVector Along = Direction * std::max(0.0f, AlongSpeed);
			FVector Slide = V2D - Along;
			const float SlideSpeed = Slide.Length();
			if (SlideSpeed > 0.0f)
			{
				const float NewSlideSpeed = std::max(0.0f, SlideSpeed - BrakingFriction * DeltaTime);
				Slide = Slide * (NewSlideSpeed / SlideSpeed);
				const FVector NewV2D = Along + Slide;
				Velocity.X = NewV2D.X;
				Velocity.Y = NewV2D.Y;
			}
		}
	}
	else if (MovementMode == EMovementMode::Walking)
	{
		// Walking 에선 input 없으면 braking. Falling 중에는 평면 속도 유지.
		FVector V2D(Velocity.X, Velocity.Y, 0.0f);
		const float Speed2D = V2D.Length();
		if (Speed2D > 0.0f)
		{
			const float NewSpeed = std::max(0.0f, Speed2D - BrakingFriction * DeltaTime);
			const FVector Dir    = V2D * (1.0f / Speed2D);
			Velocity.X = Dir.X * NewSpeed;
			Velocity.Y = Dir.Y * NewSpeed;
		}
	}

	// MaxWalkSpeed 클램프 (평면 속도만).
	FVector V2D(Velocity.X, Velocity.Y, 0.0f);
	const float Speed2D = V2D.Length();
	const float CurrentMaxWalkSpeed = GetMaxWalkSpeed();
	if (Speed2D > CurrentMaxWalkSpeed)
	{
		const FVector Dir = V2D * (1.0f / Speed2D);
		Velocity.X = Dir.X * CurrentMaxWalkSpeed;
		Velocity.Y = Dir.Y * CurrentMaxWalkSpeed;
	}
}

void UCharacterMovementComponent::TickWalking(float DeltaTime, const FVector& RootMotionWorldXY)
{
	USceneComponent* Updated = GetUpdatedComponent();

	// Jump 의도가 있으면 — Velocity.Z 박고 즉시 Falling 으로 전환. 이 frame 의 XY 는 그대로 진행.
	if (bWantsJump)
	{
		bWantsJump = false;
		GroundMissFrames = 0;
		Velocity.Z = JumpZVelocity;
		if (bEnableBuiltInMovementAudio)
		{
			FAudioManager::Get().PlayOneShot("player.jump");
		}
		// 지상 점프 1회 소비 — 남은 공중 점프 수는 MaxJumpCount - 1.
		JumpsRemaining = MaxJumpCount - 1;
		SetMovementMode(EMovementMode::Falling);
		// XY 이동은 Falling 분기로 위임 — 한 frame 안 mode 전환이라 즉시 falling tick.
		TickFalling(DeltaTime, RootMotionWorldXY);
		return;
	}

	// Walking 중 Z velocity 는 0 — floor stick 으로만 Z 결정.
	Velocity.Z = 0.0f;

	// XY 이동: input velocity * dt + root motion XY (이미 world frame).
	const FVector XYOffset(
		Velocity.X * DeltaTime + RootMotionWorldXY.X,
		Velocity.Y * DeltaTime + RootMotionWorldXY.Y,
		-FloorProbeDistance);

	const FControllerMoveResult Result = MoveController(XYOffset, DeltaTime);
	const bool bHasWalkableFloor = Result.bHasWalkableFloor;

	if (bHasWalkableFloor)
	{
		GroundMissFrames = 0;
		Velocity.Z = 0.0f;
	}
	else if (Result.bHitDown && Result.bHasFloorProbeHit)
	{
		GroundMissFrames = 0;
		// 걷다가 떨어진 경우 — 지상 점프 1회는 암묵적으로 소비된 것으로 간주 (TF2 식).
		JumpsRemaining = MaxJumpCount - 1;
		SetMovementMode(EMovementMode::Falling);
	}
	else
	{
		++GroundMissFrames;
		if (GroundMissFrames > GroundMissToleranceFrames)
		{
			GroundMissFrames = 0;
			JumpsRemaining = MaxJumpCount - 1;
			SetMovementMode(EMovementMode::Falling);
		}
	}
}

void UCharacterMovementComponent::TickFalling(float DeltaTime, const FVector& RootMotionWorldXY)
{
	USceneComponent* Updated = GetUpdatedComponent();

	// 공중 점프 소비 — gravity 전에 Z 를 박아야 같은 frame 에 즉시 상승 시작.
	if (bWantsJump && FatiguedAirJumpInputLockTimer > 0.0f)
	{
		bWantsJump = false;
	}
	if (bWantsJump && JumpsRemaining > 0)
	{
		bWantsJump = false;
		--JumpsRemaining;
		bAirJumpConsumedThisFrame = true;
		Velocity.Z = JumpZVelocity;
		if (bEnableBuiltInMovementAudio)
		{
			FAudioManager::Get().PlayOneShot("player.double_jump");
			FAudioManager::Get().PlayOneShot("player.jump.jet");
			FAudioManager::Get().FadeOutEvent("player.jump.jet", 420.0f);
		}
	}

	// Gravity — Z 만. (양수 Gravity → -Z 가속)
	Velocity.Z -= Gravity * DeltaTime;

	// Velocity * dt 의 XY 에 root motion XY 합산. Z 는 gravity 가 책임이라 root motion 무시.
	const FVector Offset(
		Velocity.X * DeltaTime + RootMotionWorldXY.X,
		Velocity.Y * DeltaTime + RootMotionWorldXY.Y,
		Velocity.Z * DeltaTime);

	const FControllerMoveResult Result = MoveController(Offset, DeltaTime);
	const bool bHasWalkableFloor = Result.bHasWalkableFloor;

	if (Velocity.Z <= 0.0f && bHasWalkableFloor)
	{
		const float LandingDownSpeed = std::max(0.0f, -Velocity.Z);
		GroundMissFrames = 0;
		Velocity.Z = 0.0f;
		// 착지 — 점프 카운트 완전 회복.
		JumpsRemaining = MaxJumpCount;
		SetMovementMode(EMovementMode::Walking);
		PlayLandingAudio(LandingDownSpeed);
	}
	else if (Result.bHitDown && !bHasWalkableFloor)
	{
		// 아래쪽 접촉은 있지만 걸을 수 없는 면 (가파른 슬로프) — PhysX 가 물리적으론 막아주는데
		// 우리가 추적하는 Velocity.Z 는 매 frame -g*dt 가 무한 누적되어 Speed 폭주 + 비현실적인
		// 슬라이드 가속을 유발. 슬라이드 터미널 속도로 clamp 해서 안정화.
		if (Velocity.Z < -MaxFallingSlideSpeed)
		{
			Velocity.Z = -MaxFallingSlideSpeed;
		}
	}
}

void UCharacterMovementComponent::BuildWallRunSweepCandidates(bool bRightSide, TArray<FVector>& OutCandidates) const
{
	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated) return;

	const FVector ActorRight = Updated->GetRightVector();
	const FVector ActorForward = Updated->GetForwardVector();

	FVector MoveForward(Velocity.X, Velocity.Y, 0.0f);
	if (MoveForward.IsNearlyZero())
	{
		MoveForward = FVector(ActorForward.X, ActorForward.Y, 0.0f);
	}
	if (!MoveForward.IsNearlyZero())
	{
		MoveForward.Normalize();
	}

	FVector MoveRight = FVector::ZeroVector;
	if (!MoveForward.IsNearlyZero())
	{
		MoveRight = FVector::UpVector.Cross(MoveForward);
		if (!MoveRight.IsNearlyZero())
		{
			MoveRight.Normalize();
		}
	}

	const float SideSign = bRightSide ? 1.0f : -1.0f;
	OutCandidates.push_back(ActorRight * SideSign);
	if (!MoveRight.IsNearlyZero())
	{
		OutCandidates.push_back(MoveRight * SideSign);
	}
	if (!MoveForward.IsNearlyZero())
	{
		OutCandidates.push_back((ActorRight * SideSign + MoveForward * 0.75f).Normalized());
		if (!MoveRight.IsNearlyZero())
		{
			OutCandidates.push_back((MoveRight * SideSign + MoveForward * 0.75f).Normalized());
		}
	}
}

bool UCharacterMovementComponent::SweepWallRunSide(bool bRightSide, FHitResult& OutHit) const
{
	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated) return false;

	TArray<FVector> Candidates;
	BuildWallRunSweepCandidates(bRightSide, Candidates);

	bool bFound = false;
	bool bFoundRunnable = false;
	float BestDistance = FLT_MAX;
	float BestRunnableDistance = FLT_MAX;
	FHitResult BestHit;
	FHitResult BestRunnableHit;
	for (const FVector& Candidate : Candidates)
	{
		if (Candidate.IsNearlyZero()) continue;

		FHitResult CandidateHit;
		if (!SweepWallRunDirection(Candidate, CandidateHit))
		{
			continue;
		}

		const FVector HitNormal = !CandidateHit.WorldNormal.IsNearlyZero() ? CandidateHit.WorldNormal : CandidateHit.ImpactNormal;
		if (IsRunnableWall(HitNormal) && CandidateHit.Distance < BestRunnableDistance)
		{
			BestRunnableDistance = CandidateHit.Distance;
			BestRunnableHit = CandidateHit;
			bFoundRunnable = true;
		}

		if (CandidateHit.Distance >= BestDistance) continue;

		BestDistance = CandidateHit.Distance;
		BestHit = CandidateHit;
		bFound = true;
	}

	if (!bFound)
	{
		return false;
	}

	if (bFoundRunnable)
	{
		OutHit = BestRunnableHit;
		return true;
	}

	OutHit = BestHit;
	return true;
}

bool UCharacterMovementComponent::SweepWallRunDirection(const FVector& Direction, FHitResult& OutHit) const
{
	UWorld* World = GetWorld();
	USceneComponent* Updated = GetUpdatedComponent();
	if (!World || !Updated)
	{
		OutHit = FHitResult{};
		return false;
	}

	FVector SweepDirection = Direction;
	if (SweepDirection.IsNearlyZero())
	{
		OutHit = FHitResult{};
		return false;
	}
	SweepDirection.Normalize();

	const FVector Start = GetWallRunSweepStart(SweepDirection);
	const bool bEmitDiagnostics = ShouldEmitWallRunDiagnostics();

	FHitResult PhysXHit;
	bool bPhysXHit = World->PhysicsSweepSphere(
		Start,
		SweepDirection,
		WallCheckDistance,
		WallCheckSphereRadius,
		PhysXHit,
		ECollisionChannel::WorldStatic,
		GetOwner());

	FHitResult ShapeHit;
	bool bShapeHit = World->PhysicsSphereSweepShapeComponents(
		Start,
		SweepDirection,
		WallCheckDistance,
		WallCheckSphereRadius,
		ShapeHit,
		ECollisionChannel::WorldStatic,
		GetOwner());

	auto NormalizeSphereSweepHit = [&](bool& bHit, FHitResult& Hit)
	{
		if (!bHit) return;

		Hit.Distance = std::max(0.0f, Hit.Distance + WallCheckSphereRadius);
		if (Hit.Distance > WallCheckDistance)
		{
			bHit = false;
			Hit = FHitResult{};
		}
	};

	NormalizeSphereSweepHit(bPhysXHit, PhysXHit);
	NormalizeSphereSweepHit(bShapeHit, ShapeHit);

	FHitResult MeshTraceHit;
	const bool bMeshTraceHit = SweepWallRunStaticMeshes(
		Start,
		SweepDirection,
		MeshTraceHit);

	// Bounds hit 는 실제 삼각형/충돌 표면이 아니라 StaticMeshComponent 의 전체 AABB 이다.
	// 큰 레벨 메시나 복합 프롭의 빈 공간에서도 수평 normal 을 가진 "가짜 벽"이 만들어져
	// 허공에서 벽타기 시작 -> 즉시 ENDED_NO_WALL 로 끝나는 원인이 된다.
	// 따라서 벽타기 후보로는 사용하지 않고, diagnostics 로그에서만 관찰한다.
	FHitResult BoundsHit;
	bool bBoundsHit = false;

	FHitResult ObjectRayHit;
	bool bObjectRayHit = false;
	if (bEmitDiagnostics)
	{
		bBoundsHit = SweepWallRunStaticMeshBounds(
			Start,
			SweepDirection,
			BoundsHit);

		bObjectRayHit = World->PhysicsRaycastByObjectTypes(
			Start,
			SweepDirection,
			WallCheckDistance,
			ObjectRayHit,
			ObjectTypeBit(ECollisionChannel::WorldStatic),
			GetOwner());
	}

	bool bFound = false;
	bool bFoundRunnable = false;
	float BestDistance = FLT_MAX;
	float BestRunnableDistance = FLT_MAX;
	FHitResult BestHit;
	FHitResult BestRunnableHit;
	const char* BestSource = "None";
	const char* BestRunnableSource = "None";

	auto ConsiderHit = [&](bool bHit, const FHitResult& Hit, const char* Source)
	{
		if (!bHit) return;
		if (!IsWallRunAllowedHit(Hit)) return;

		const float CandidateDistance = Hit.Distance;
		if (CandidateDistance < 0.0f || CandidateDistance > WallCheckDistance) return;

		if (CandidateDistance < BestDistance)
		{
			BestDistance = CandidateDistance;
			BestHit = Hit;
			BestSource = Source;
			bFound = true;
		}

		const FVector HitNormal = !Hit.WorldNormal.IsNearlyZero() ? Hit.WorldNormal : Hit.ImpactNormal;
		if (IsRunnableWall(HitNormal) && CandidateDistance < BestRunnableDistance)
		{
			BestRunnableDistance = CandidateDistance;
			BestRunnableHit = Hit;
			BestRunnableSource = Source;
			bFoundRunnable = true;
		}
	};

	ConsiderHit(bPhysXHit, PhysXHit, "PhysX");
	ConsiderHit(bShapeHit, ShapeHit, "Shape");
	ConsiderHit(bMeshTraceHit, MeshTraceHit, "MeshTrace");

	if (bEmitDiagnostics)
	{
		const bool bSelected = bFoundRunnable || bFound;
		const FHitResult& SelectedHit = bFoundRunnable ? BestRunnableHit : BestHit;
		const FVector SelectedNormal = bSelected ? GetHitNormal(SelectedHit) : FVector::ZeroVector;
		const float SelectedUpDot = SelectedNormal.IsNearlyZero() ? 0.0f : SelectedNormal.Normalized().Dot(FVector::UpVector);
		const FString SelectedActor = GetHitActorName(SelectedHit);
		const FString SelectedComponent = GetHitComponentName(SelectedHit);
		const FString ObjectActor = GetHitActorName(ObjectRayHit);
		const FString ObjectComponent = GetHitComponentName(ObjectRayHit);

		UE_LOG(
			"[WallRunProbe] selected=%s runnable=%s source=%s start=(%.2f,%.2f,%.2f) dir=(%.2f,%.2f,%.2f) check=%.2f radius=%.2f d=%.2f normal=(%.2f,%.2f,%.2f) upDot=%.2f actor=%s comp=%s",
			BoolText(bSelected),
			BoolText(bFoundRunnable),
			bFoundRunnable ? BestRunnableSource : BestSource,
			Start.X,
			Start.Y,
			Start.Z,
			SweepDirection.X,
			SweepDirection.Y,
			SweepDirection.Z,
			WallCheckDistance,
			WallCheckSphereRadius,
			HitDistanceOrMiss(bSelected, SelectedHit),
			SelectedNormal.X,
			SelectedNormal.Y,
			SelectedNormal.Z,
			SelectedUpDot,
			SelectedActor.c_str(),
			SelectedComponent.c_str());

		UE_LOG(
			"[WallRunProbeSources] phys=%s/%.2f shape=%s/%.2f mesh=%s/%.2f bounds=%s/%.2f objectRay=%s/%.2f objectActor=%s objectComp=%s",
			BoolText(bPhysXHit),
			HitDistanceOrMiss(bPhysXHit, PhysXHit),
			BoolText(bShapeHit),
			HitDistanceOrMiss(bShapeHit, ShapeHit),
			BoolText(bMeshTraceHit),
			HitDistanceOrMiss(bMeshTraceHit, MeshTraceHit),
			BoolText(bBoundsHit),
			HitDistanceOrMiss(bBoundsHit, BoundsHit),
			BoolText(bObjectRayHit),
			HitDistanceOrMiss(bObjectRayHit, ObjectRayHit),
			ObjectActor.c_str(),
			ObjectComponent.c_str());
	}

	if (bFoundRunnable)
	{
		OutHit = BestRunnableHit;
		return true;
	}
	if (bFound)
	{
		OutHit = BestHit;
		return true;
	}

	OutHit = FHitResult{};
	return false;
}

bool UCharacterMovementComponent::SweepWallRunStaticMeshes(const FVector& Start, const FVector& Direction, FHitResult& OutHit) const
{
	UWorld* World = GetWorld();
	if (!World) return false;

	FVector RayDirection = Direction;
	if (RayDirection.IsNearlyZero()) return false;
	RayDirection.Normalize();

	FRay Ray;
	Ray.Origin = Start;
	Ray.Direction = RayDirection;

	bool bFound = false;
	float BestDistance = FLT_MAX;
	FHitResult BestHit;

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor) continue;
		if (Actor == GetOwner()) continue;
		if (!IsWallRunAllowedActor(Actor)) continue;

		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Primitive);
			if (!StaticMeshComponent) continue;
			if (!StaticMeshComponent->IsQueryCollisionEnabled()) continue;
			if (StaticMeshComponent->GetCollisionResponseToChannel(ECollisionChannel::WorldStatic) != ECollisionResponse::Block) continue;

			const FBoundingBox Bounds = ExpandBounds(StaticMeshComponent->GetWorldBoundingBox(), WallCheckSphereRadius);
			float TMin = 0.0f;
			float TMax = 0.0f;
			if (!FRayUtils::IntersectRayAABB(Ray, Bounds.Min, Bounds.Max, TMin, TMax)) continue;
			if (TMin > BestDistance) continue;

			FHitResult CandidateHit;
			if (!StaticMeshComponent->LineTraceComponent(Ray, CandidateHit)) continue;
			if (CandidateHit.Distance > WallCheckDistance) continue;
			if (CandidateHit.Distance >= BestDistance) continue;
			if (!FillStaticMeshTraceHit(StaticMeshComponent, Start, RayDirection, CandidateHit)) continue;

			BestDistance = CandidateHit.Distance;
			BestHit = CandidateHit;
			bFound = true;
		}
	}

	if (!bFound)
	{
		OutHit = FHitResult{};
		return false;
	}

	OutHit = BestHit;
	return true;
}

bool UCharacterMovementComponent::SweepWallRunStaticMeshBounds(const FVector& Start, const FVector& Direction, FHitResult& OutHit) const
{
	UWorld* World = GetWorld();
	if (!World) return false;

	FVector RayDirection = Direction;
	if (RayDirection.IsNearlyZero()) return false;
	RayDirection.Normalize();

	FRay Ray;
	Ray.Origin = Start;
	Ray.Direction = RayDirection;

	bool bFound = false;
	float BestDistance = FLT_MAX;
	FHitResult BestHit;

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor) continue;
		if (Actor == GetOwner()) continue;
		if (!IsWallRunAllowedActor(Actor)) continue;

		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Primitive);
			if (!StaticMeshComponent) continue;
			if (!StaticMeshComponent->IsQueryCollisionEnabled()) continue;
			if (StaticMeshComponent->GetCollisionResponseToChannel(ECollisionChannel::WorldStatic) != ECollisionResponse::Block) continue;

			const FBoundingBox MeshBounds = StaticMeshComponent->GetWorldBoundingBox();
			if (!MeshBounds.IsValid()) continue;

			const FBoundingBox QueryBounds = ExpandBounds(MeshBounds, WallCheckSphereRadius);
			float TMin = 0.0f;
			float TMax = 0.0f;
			if (!FRayUtils::IntersectRayAABB(Ray, QueryBounds.Min, QueryBounds.Max, TMin, TMax)) continue;
			if (TMin > BestDistance) continue;

			FHitResult CandidateHit;
			if (!BuildBoundsWallHit(
				StaticMeshComponent,
				MeshBounds,
				Start,
				RayDirection,
				WallCheckDistance,
				CandidateHit))
			{
				continue;
			}

			const FVector HitNormal = !CandidateHit.WorldNormal.IsNearlyZero() ? CandidateHit.WorldNormal : CandidateHit.ImpactNormal;
			if (!IsRunnableWall(HitNormal)) continue;
			if (CandidateHit.Distance >= BestDistance) continue;

			BestDistance = CandidateHit.Distance;
			BestHit = CandidateHit;
			bFound = true;
		}
	}

	if (!bFound)
	{
		OutHit = FHitResult{};
		return false;
	}

	OutHit = BestHit;
	return true;
}

bool UCharacterMovementComponent::FindWallRunSurface(FHitResult& OutHit, bool& bOutRightSide) const
{
	FHitResult RightHit;
	FHitResult LeftHit;
	const bool bHasRightWall = SweepWallRunSide(true, RightHit);
	const bool bHasLeftWall = SweepWallRunSide(false, LeftHit);
	const bool bRightRunnable = bHasRightWall && IsRunnableWall(!RightHit.WorldNormal.IsNearlyZero() ? RightHit.WorldNormal : RightHit.ImpactNormal);
	const bool bLeftRunnable = bHasLeftWall && IsRunnableWall(!LeftHit.WorldNormal.IsNearlyZero() ? LeftHit.WorldNormal : LeftHit.ImpactNormal);

	if (ShouldEmitWallRunDiagnostics())
	{
		const FVector RightNormal = bHasRightWall ? GetHitNormal(RightHit) : FVector::ZeroVector;
		const FVector LeftNormal = bHasLeftWall ? GetHitNormal(LeftHit) : FVector::ZeroVector;
		const FString RightActor = GetHitActorName(RightHit);
		const FString LeftActor = GetHitActorName(LeftHit);

		UE_LOG(
			"[WallRunFind] right=%s runnable=%s d=%.2f n=(%.2f,%.2f,%.2f) actor=%s | left=%s runnable=%s d=%.2f n=(%.2f,%.2f,%.2f) actor=%s",
			BoolText(bHasRightWall),
			BoolText(bRightRunnable),
			HitDistanceOrMiss(bHasRightWall, RightHit),
			RightNormal.X,
			RightNormal.Y,
			RightNormal.Z,
			RightActor.c_str(),
			BoolText(bHasLeftWall),
			BoolText(bLeftRunnable),
			HitDistanceOrMiss(bHasLeftWall, LeftHit),
			LeftNormal.X,
			LeftNormal.Y,
			LeftNormal.Z,
			LeftActor.c_str());
	}

	if (!bHasRightWall && !bHasLeftWall)
	{
		return false;
	}

	if (bRightRunnable || bLeftRunnable)
	{
		if (bRightRunnable && (!bLeftRunnable || RightHit.Distance <= LeftHit.Distance))
		{
			OutHit = RightHit;
			bOutRightSide = true;
			return true;
		}

		OutHit = LeftHit;
		bOutRightSide = false;
		return true;
	}

	// 양쪽이 동시에 잡히면 더 가까운 벽을 선택 — 좁은 통로에서 흔들림을 줄이기 위한 단순 기준.
	if (bHasRightWall && (!bHasLeftWall || RightHit.Distance <= LeftHit.Distance))
	{
		OutHit = RightHit;
		bOutRightSide = true;
		return true;
	}

	OutHit = LeftHit;
	bOutRightSide = false;
	return true;
}

bool UCharacterMovementComponent::IsWallRunAllowedActor(const AActor* Actor) const
{
	// 필수 태그가 비어 있으면 기존처럼 모든 runnable wall actor 허용
	if (!WallRunRequiredTag.IsValid() || WallRunRequiredTag == FName::None)
	{
		return true;
	}

	return Actor && Actor->HasTag(WallRunRequiredTag);
}

bool UCharacterMovementComponent::IsWallRunAllowedHit(const FHitResult& Hit) const
{
	if (!WallRunRequiredTag.IsValid() || WallRunRequiredTag == FName::None)
	{
		return true;
	}

	const AActor* HitActor = Hit.HitActor;
	if (!HitActor && Hit.HitComponent)
	{
		HitActor = Hit.HitComponent->GetOwner();
	}

	return IsWallRunAllowedActor(HitActor);
}

bool UCharacterMovementComponent::IsRunnableWall(const FVector& WallNormal) const
{
	if (WallNormal.IsNearlyZero()) return false;

	const FVector Normal = WallNormal.Normalized();
	const float AbsUpDot = std::fabs(Normal.Dot(FVector::UpVector));

	// 1차: 명시 threshold — 거의 수직 면. 보통 RunnableWallUpDot 가 작게 잡혀 있어
	//      45°~78° 사이의 "가파르지만 완전 수직은 아닌" 슬로프는 여기서 잘려나간다.
	if (AbsUpDot <= RunnableWallUpDot) return true;

	// 2차: walkable 의 여집합. WalkableSlopeAngle 보다 가파른 면은 PhysX 가 floor 로 안 잡고
	//      Falling 이 유지되므로, 같은 면을 wall-run 후보로 받아주지 않으면 "걷지도 못하고
	//      벽타기도 안 되는" 사각지대가 생긴다. 천장 (UpDot < 0) 까지 같은 식으로 메운다.
	const float WalkableLimit = std::cos(WalkableSlopeAngle * FMath::DegToRad);
	return AbsUpDot < WalkableLimit;
}

FVector UCharacterMovementComponent::ComputeWallRunDirection(const FVector& WallNormal) const
{
	if (WallNormal.IsNearlyZero()) return FVector::ZeroVector;

	// 벽 진행 방향 — 월드 위쪽과 벽 normal 에 모두 수직인 접선.
	FVector Direction = FVector::UpVector.Cross(WallNormal.Normalized());
	if (Direction.IsNearlyZero()) return FVector::ZeroVector;
	Direction.Normalize();

	// 현재 이동 방향과 더 가까운 접선을 선택해야 같은 벽에서도 뒤집히지 않고 진행 방향이 유지된다.
	const FVector PlanarVelocity(Velocity.X, Velocity.Y, 0.0f);
	if (!PlanarVelocity.IsNearlyZero() && Direction.Dot(PlanarVelocity) < 0.0f)
	{
		Direction = Direction * -1.0f;
	}

	return Direction;
}

bool UCharacterMovementComponent::TryStartWallRun(const FVector& Input)
{
	if (!bEnableWallRun)
	{
		SetWallRunStatus(EWallRunStatus::Disabled);
		return false;
	}
	if (MovementMode != EMovementMode::Falling)
	{
		SetWallRunStatus(EWallRunStatus::NotFalling);
		return false;
	}
	if (WallRunFatigueTimer > 0.0f)
	{
		SetWallRunStatus(EWallRunStatus::Fatigued);
		return false;
	}

	FHitResult WallHit;
	bool bRightSide = false;
	if (!FindWallRunSurface(WallHit, bRightSide))
	{
		SetWallRunStatus(EWallRunStatus::NoWall);
		return false;
	}

	const FVector PlanarVelocity(Velocity.X, Velocity.Y, 0.0f);
	if (PlanarVelocity.Length() < MinWallRunStartSpeed)
	{
		SetWallRunStatus(EWallRunStatus::LowSpeed, &WallHit);
		return false;
	}

	const FVector WallNormal = !WallHit.WorldNormal.IsNearlyZero() ? WallHit.WorldNormal : WallHit.ImpactNormal;
	if (!IsRunnableWall(WallNormal))
	{
		SetWallRunStatus(EWallRunStatus::BadNormal, &WallHit);
		return false;
	}

	// Wall-jump 쿨다운 동안 같은 normal 의 벽 후보는 거절 — 핑퐁 방지.
	// 반대편 벽 (normal 반대) 은 dot 가 음수라 |dot| 비교로 부호 무시.
	if (WallJumpReattachTimer > 0.0f && !LastWallJumpNormal.IsNearlyZero())
	{
		const FVector CurrentNormal = WallNormal.Normalized();
		if (std::fabs(CurrentNormal.Dot(LastWallJumpNormal)) >= WallJumpReattachNormalDot)
		{
			SetWallRunStatus(EWallRunStatus::BadNormal, &WallHit);
			return false;
		}
	}

	const FVector RunDirection = ComputeWallRunDirection(WallNormal);
	if (RunDirection.IsNearlyZero())
	{
		SetWallRunStatus(EWallRunStatus::BadDirection, &WallHit);
		return false;
	}

	const float AlongWallSpeed = std::fabs(PlanarVelocity.Dot(RunDirection));
	if (AlongWallSpeed < MinWallRunStartSpeed)
	{
		if (ShouldEmitWallRunDiagnostics())
		{
			UE_LOG(
				"[WallRun] reject=LowAlongSpeed total=%.2f along=%.2f min=%.2f normal=(%.2f,%.2f,%.2f) runDir=(%.2f,%.2f,%.2f) hitD=%.2f actor=%s comp=%s",
				PlanarVelocity.Length(),
				AlongWallSpeed,
				MinWallRunStartSpeed,
				WallNormal.X,
				WallNormal.Y,
				WallNormal.Z,
				RunDirection.X,
				RunDirection.Y,
				RunDirection.Z,
				WallHit.Distance,
				GetHitActorName(WallHit).c_str(),
				GetHitComponentName(WallHit).c_str());
		}
		SetWallRunStatus(EWallRunStatus::BadDirection, &WallHit);
		return false;
	}

	// 시작 단계에서도 입력 의도를 검사한다. 입력이 벽 진행 방향이 아니면
	// Falling -> Active -> EndedBadDirection 루프가 매 프레임 반복되어 벽에 붙어 보일 수 있다.
	const float InputAlongWall = GetWallRunInputAlong(Input, RunDirection);
	if (std::fabs(InputAlongWall) < MinWallRunInputAlong)
	{
		if (ShouldEmitWallRunDiagnostics())
		{
			UE_LOG(
				"[WallRunStart] reject=NoAlongInput input=(%.2f,%.2f,%.2f) runDir=(%.2f,%.2f,%.2f) normal=(%.2f,%.2f,%.2f) hitD=%.2f actor=%s comp=%s",
				Input.X,
				Input.Y,
				Input.Z,
				RunDirection.X,
				RunDirection.Y,
				RunDirection.Z,
				WallNormal.X,
				WallNormal.Y,
				WallNormal.Z,
				WallHit.Distance,
				GetHitActorName(WallHit).c_str(),
				GetHitComponentName(WallHit).c_str());
		}

		SetWallRunStatus(EWallRunStatus::BadDirection, &WallHit);
		return false;
	}

	StartWallRun(WallHit, bRightSide);
	return true;
}

void UCharacterMovementComponent::StartWallRun(const FHitResult& WallHit, bool bRightSide)
{
	const FVector RawNormal = !WallHit.WorldNormal.IsNearlyZero() ? WallHit.WorldNormal : WallHit.ImpactNormal;
	if (RawNormal.IsNearlyZero())
	{
		SetWallRunStatus(EWallRunStatus::BadNormal, &WallHit);
		return;
	}

	WallRunNormal = RawNormal.Normalized();
	WallRunDirection = ComputeWallRunDirection(WallRunNormal);
	if (WallRunDirection.IsNearlyZero())
	{
		SetWallRunStatus(EWallRunStatus::BadDirection, &WallHit);
		return;
	}

	WallRunElapsedTime = 0.0f;
	bWallRunOnRightSide = bRightSide;

	// 벽 안쪽으로 파고드는 속도 제거 — CCT 가 벽과 싸우지 않고 표면 접선 속도만 유지하도록 정리.
	Velocity = Velocity - WallRunNormal * Velocity.Dot(WallRunNormal);

	const FVector PlanarVelocity(Velocity.X, Velocity.Y, 0.0f);
	const float ExistingForwardSpeed = std::fabs(PlanarVelocity.Dot(WallRunDirection));
	const float MinRunSpeed = (std::min)(WallRunMinSpeed, WallRunMaxSpeed);
	const float StartSpeed = FMath::Clamp((std::max)(ExistingForwardSpeed, MinRunSpeed), 0.0f, WallRunMaxSpeed);

	Velocity.X = WallRunDirection.X * StartSpeed;
	Velocity.Y = WallRunDirection.Y * StartSpeed;

	// 진입 직후에는 아래로 꺼지는 느낌을 줄여 벽을 믿고 달릴 수 있게 한다.
	if (Velocity.Z < 0.0f)
	{
		Velocity.Z = 0.0f;
	}

	SetMovementMode(EMovementMode::WallRunning);
	WallRunStepDistance = 0.0f;
	if (bEnableBuiltInMovementAudio)
	{
		FAudioManager::Get().StopEvent("player.jump.jet");
	}
	SetWallRunStatus(EWallRunStatus::Active, &WallHit);
}

void UCharacterMovementComponent::EndWallRun()
{
	if (bEnableBuiltInMovementAudio)
	{
		FAudioManager::Get().SetLoopState("PlayerWallRunRub", "WallRunRub", false);
	}

	WallRunNormal = FVector::ZeroVector;
	WallRunDirection = FVector::ZeroVector;
	WallRunElapsedTime = 0.0f;
	bWallRunOnRightSide = false;

	if (MovementMode == EMovementMode::WallRunning)
	{
		SetMovementMode(EMovementMode::Falling);
	}
}

void UCharacterMovementComponent::BeginWallRunFatigue()
{
	WallRunFatigueTimer = std::max(0.0f, WallRunFatigueDuration);
	FatiguedAirJumpInputLockTimer = std::max(0.0f, FatiguedAirJumpInputLockDuration);
	JumpsRemaining = std::max(JumpsRemaining, MaxJumpCount - 1);
}

void UCharacterMovementComponent::PerformWallJump()
{
	// 현재 wall-run 의 normal/direction 을 임펄스 기준으로 사용 — EndWallRun 이 비우기 전에 캐싱.
	const FVector JumpNormal    = !WallRunNormal.IsNearlyZero()    ? WallRunNormal.Normalized()    : FVector::UpVector;
	const FVector JumpDirection = !WallRunDirection.IsNearlyZero() ? WallRunDirection.Normalized() : FVector::ZeroVector;

	// 세 성분 합성: 벽에서 밀려나기 + 위 + 진행 방향 보너스.
	const FVector NewVelocity =
		JumpNormal    * WallJumpOutVelocity +
		FVector::UpVector * WallJumpUpVelocity +
		JumpDirection * WallJumpForwardVelocity;

	Velocity = NewVelocity;

	// 같은 벽 즉시 재진입 방지용 상태 기록.
	LastWallJumpNormal      = JumpNormal;
	WallJumpReattachTimer   = WallJumpReattachCooldown;

	// 에어 점프 카운트 리필 — wall-jump 후에도 더블점프 1회 더 가능 (TF2 콤보 느낌).
	JumpsRemaining = std::max(JumpsRemaining, MaxJumpCount - 1);

	if (bLogWallRunDiagnostics && ShouldEmitWallRunDiagnostics())
	{
		UE_LOG(
			"[WallJump] outV=%.2f upV=%.2f fwdV=%.2f normal=(%.2f,%.2f,%.2f) dir=(%.2f,%.2f,%.2f) jumpsRemaining=%d",
			WallJumpOutVelocity,
			WallJumpUpVelocity,
			WallJumpForwardVelocity,
			JumpNormal.X, JumpNormal.Y, JumpNormal.Z,
			JumpDirection.X, JumpDirection.Y, JumpDirection.Z,
			JumpsRemaining);
	}

	EndWallRun();
}

void UCharacterMovementComponent::TickWallRunning(float DeltaTime, const FVector& RootMotionWorldXY, const FVector& Input)
{
	WallRunElapsedTime += DeltaTime;
	if (bEnableBuiltInMovementAudio && WallRunElapsedTime >= WallRunLoopStartDelay)
	{
		FAudioManager::Get().SetLoopState("PlayerWallRunRub", "WallRunRub", true, 0.48f, 1.0f);
	}

	const FVector WallPlanarVelocity(Velocity.X, Velocity.Y, 0.0f);
	WallRunStepDistance += WallPlanarVelocity.Length() * DeltaTime;
	if (WallRunStepDistance >= WallRunStepStrideDistance)
	{
		WallRunStepDistance = std::fmod(WallRunStepDistance, WallRunStepStrideDistance);
		if (bEnableBuiltInMovementAudio)
		{
			FAudioManager::Get().PlayOneShot("player.wallrun.step");
		}
	}

	// 점프 의도가 있으면 wall-jump 임펄스로 변환하고 즉시 Falling 으로 — 이번 frame 의
	// gravity/벽 sweep 은 건너뛴다. 같은 frame 안 mode 전환이므로 다음 frame TickFalling 이 받음.
	if (bWantsJump)
	{
		bWantsJump = false;
		PerformWallJump();
		return;
	}

	if (MaxWallRunTime > 0.0f && WallRunElapsedTime > MaxWallRunTime)
	{
		SetWallRunStatus(EWallRunStatus::EndedTimeLimit);
		BeginWallRunFatigue();
		EndWallRun();
		return;
	}

	FHitResult WallHit;
	bool bRightSide = bWallRunOnRightSide;

	// 먼저 현재 타던 쪽을 유지해서 좌우 전환 흔들림을 줄이고, 실패하면 양쪽 후보를 다시 찾는다.
	bool bHasWall = SweepWallRunSide(bWallRunOnRightSide, WallHit);
	if (!bHasWall)
	{
		bHasWall = FindWallRunSurface(WallHit, bRightSide);
	}
	if (!bHasWall)
	{
		SetWallRunStatus(EWallRunStatus::EndedNoWall);
		EndWallRun();
		return;
	}

	const FVector RawNormal = !WallHit.WorldNormal.IsNearlyZero() ? WallHit.WorldNormal : WallHit.ImpactNormal;
	if (!IsRunnableWall(RawNormal))
	{
		SetWallRunStatus(EWallRunStatus::EndedBadNormal, &WallHit);
		EndWallRun();
		return;
	}

	WallRunNormal = RawNormal.Normalized();
	WallRunDirection = ComputeWallRunDirection(WallRunNormal);
	bWallRunOnRightSide = bRightSide;
	if (WallRunDirection.IsNearlyZero())
	{
		SetWallRunStatus(EWallRunStatus::EndedBadDirection, &WallHit);
		EndWallRun();
		return;
	}

	float InputAlongWall = GetWallRunInputAlong(Input, WallRunDirection);
	if (std::fabs(InputAlongWall) < MinWallRunInputAlong)
	{
		if (ShouldEmitWallRunDiagnostics())
		{
			UE_LOG(
				"[WallRunInput] reject=NoAlongInput input=(%.2f,%.2f,%.2f) runDir=(%.2f,%.2f,%.2f) normal=(%.2f,%.2f,%.2f) hitD=%.2f actor=%s comp=%s",
				Input.X,
				Input.Y,
				Input.Z,
				WallRunDirection.X,
				WallRunDirection.Y,
				WallRunDirection.Z,
				WallRunNormal.X,
				WallRunNormal.Y,
				WallRunNormal.Z,
				WallHit.Distance,
				GetHitActorName(WallHit).c_str(),
				GetHitComponentName(WallHit).c_str());
		}

		SetWallRunStatus(EWallRunStatus::EndedBadDirection, &WallHit);
		EndWallRun();
		return;
	}

	if (InputAlongWall < 0.0f)
	{
		WallRunDirection = WallRunDirection * -1.0f;
		InputAlongWall = -InputAlongWall;
	}

	SetWallRunStatus(EWallRunStatus::Active, &WallHit);

	// 벽 normal 방향 성분 제거 후, 입력이 실린 벽 접선 방향으로만 속도를 갱신한다.
	Velocity = Velocity - WallRunNormal * Velocity.Dot(WallRunNormal);

	const FVector PlanarVelocity(Velocity.X, Velocity.Y, 0.0f);
	float ForwardSpeed = PlanarVelocity.Dot(WallRunDirection);
	if (ForwardSpeed < 0.0f)
	{
		ForwardSpeed = 0.0f;
	}

	const float MinRunSpeed = (std::min)(WallRunMinSpeed, WallRunMaxSpeed);
	const float Accel = WallRunAcceleration * InputAlongWall * DeltaTime;
	float NewForwardSpeed = ForwardSpeed + Accel;
	NewForwardSpeed = FMath::Clamp(NewForwardSpeed, MinRunSpeed, WallRunMaxSpeed);

	Velocity.X = WallRunDirection.X * NewForwardSpeed;
	Velocity.Y = WallRunDirection.Y * NewForwardSpeed;

	// 벽타기 중력 — 일반 낙하보다 약하게 적용하고, 하강 속도는 제한해 바로 떨어지는 느낌을 줄인다.
	Velocity.Z -= Gravity * WallRunGravityScale * DeltaTime;
	if (Velocity.Z < -MaxWallRunSlideSpeed)
	{
		Velocity.Z = -MaxWallRunSlideSpeed;
	}

	// 약한 벽 부착 속도 — 너무 강하면 자석처럼 느껴지므로 작은 보정만 이동 delta 에 섞는다.
	const FVector StickVelocity = WallRunNormal * -WallStickAcceleration;

	const FVector Offset(
		Velocity.X * DeltaTime + RootMotionWorldXY.X + StickVelocity.X * DeltaTime,
		Velocity.Y * DeltaTime + RootMotionWorldXY.Y + StickVelocity.Y * DeltaTime,
		Velocity.Z * DeltaTime);

	const FControllerMoveResult Result = MoveController(Offset, DeltaTime);
	if (Velocity.Z <= 0.0f && Result.bHasWalkableFloor)
	{
		const float LandingDownSpeed = std::max(0.0f, -Velocity.Z);
		SetWallRunStatus(EWallRunStatus::Landed);
		WallRunNormal = FVector::ZeroVector;
		WallRunDirection = FVector::ZeroVector;
		WallRunElapsedTime = 0.0f;
		bWallRunOnRightSide = false;
		Velocity.Z = 0.0f;
		JumpsRemaining = MaxJumpCount;
		SetMovementMode(EMovementMode::Walking);
		PlayLandingAudio(LandingDownSpeed);
	}
}

void UCharacterMovementComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << MaxWalkSpeed;
	Ar << MaxAcceleration;
	Ar << BrakingFriction;
	Ar << Gravity;
	Ar << FloorProbeDistance;
	Ar << JumpZVelocity;
	Ar << MaxJumpCount;
	Ar << MaxFallingSlideSpeed;
	Ar << WallJumpOutVelocity;
	Ar << WallJumpUpVelocity;
	Ar << WallJumpForwardVelocity;
	Ar << WallJumpReattachCooldown;
	Ar << WallJumpReattachNormalDot;
	Ar << bOrientRotationToMovement;
	Ar << RotationYawRate;
	Ar << ControllerContactOffset;
	Ar << MaxStepHeight;
	Ar << WalkableSlopeAngle;
	Ar << ControllerMinMoveDistance;
	Ar << GroundMissToleranceFrames;
	Ar << ControllerSyncTeleportDistance;
	Ar << bEnableWallRun;
	Ar << WallRunRequiredTag;
	Ar << WallCheckDistance;
	Ar << WallCheckSphereRadius;
	Ar << RunnableWallUpDot;
	Ar << MinWallRunStartSpeed;
	Ar << WallRunMinSpeed;
	Ar << WallRunMaxSpeed;
	Ar << WallRunAcceleration;
	Ar << WallRunGravityScale;
	Ar << MaxWallRunSlideSpeed;
	Ar << WallStickAcceleration;
	Ar << MaxWallRunTime;
	Ar << WallRunFatigueDuration;
	Ar << FatiguedAirJumpInputLockDuration;
	Ar << bShowWallRunStatusText;
	Ar << bLogWallRunDiagnostics;
	Ar << WallRunDiagnosticsInterval;
}
