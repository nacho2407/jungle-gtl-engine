#include "VehicleMovementComponentTank.h"

#include "Component/SceneComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Object/Object.h"
#include "Physics/PhysicsScene.h"
#include "Physics/PhysXVehicleManager.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace
{
	FString MakeWheelVisualName(const FString& Prefix, const uint32 Index)
	{
		return Prefix + std::to_string(Index);
	}

	float Clamp01(const float Value)
	{
		return FMath::Clamp(Value, 0.0f, 1.0f);
	}

	float SmoothStep(const float Alpha)
	{
		const float T = Clamp01(Alpha);
		return T * T * (3.0f - 2.0f * T);
	}
}

void UVehicleMovementComponentTank::SetDriveInput(const float Throttle, const float Brake, const float Steer, const bool bReverse) const
{
	if (VehicleInstance)
	{
		VehicleInstance->SetDriveInput(Throttle, Brake, Steer, bReverse);
	}
}

void UVehicleMovementComponentTank::SetTrackInput(const float LeftThrust, const float RightThrust,
	const float LeftBrake, const float RightBrake) const
{
	if (VehicleInstance)
	{
		VehicleInstance->SetTrackInput(LeftThrust, RightThrust, LeftBrake, RightBrake);
	}
}

void UVehicleMovementComponentTank::FireRecoil(const float Impulse, const FVector LocalFirePoint, const FVector LocalDirection) const
{
	if (VehicleInstance)
	{
		VehicleInstance->FireRecoil(Impulse, LocalFirePoint, LocalDirection);
	}
}

void UVehicleMovementComponentTank::SetTurretInput(const float YawInput)
{
	TurretYawInput = FMath::Clamp(YawInput, -1.0f, 1.0f);
}

void UVehicleMovementComponentTank::SetTurretYaw(const float YawDegrees)
{
	TurretYawDegrees = ClampTurretYaw(YawDegrees);
	ApplyTurretVisual();
}

float UVehicleMovementComponentTank::GetTurretYaw() const
{
	return TurretYawDegrees;
}

FVector UVehicleMovementComponentTank::GetTurretForward() const
{
	if (IsAliveObject(TurretVisualComponent))
	{
		return TurretVisualComponent->GetForwardVector();
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return FVector::ForwardVector;
	}

	return OwnerActor->GetActorRotation().ToQuaternion().RotateVector(GetTurretYawRotation().RotateVector(FVector::ForwardVector));
}

void UVehicleMovementComponentTank::FireTurretRecoil(const float Impulse,
	const FVector TurretLocalFirePoint, const FVector TurretLocalDirection) const
{
	if (!VehicleInstance)
	{
		return;
	}

	FVector VehicleLocalFirePoint;
	FVector VehicleLocalDirection;
	if (GetTurretLocalFireTransform(TurretLocalFirePoint, TurretLocalDirection,
		VehicleLocalFirePoint, VehicleLocalDirection))
	{
		VehicleInstance->FireRecoil(Impulse, VehicleLocalFirePoint, VehicleLocalDirection);
	}
}

void UVehicleMovementComponentTank::FireMainGun()
{
	bBarrelRecoilActive = true;
	BarrelRecoilElapsed = 0.0f;
	ApplyBarrelVisual();

	if (MainGunSetup.bApplyChassisRecoil && MainGunSetup.ChassisRecoilImpulse > 0.0f)
	{
		FireTurretRecoil(MainGunSetup.ChassisRecoilImpulse,
			MainGunSetup.TurretLocalFirePoint,
			MainGunSetup.TurretLocalFireDirection);
	}
}

void UVehicleMovementComponentTank::BeginPlay()
{
	UPhysXVehicleMovementComponent::BeginPlay();
	RebuildVisualBindings();
}

void UVehicleMovementComponentTank::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction& ThisTickFunction)
{
	UPhysXVehicleMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateTurretYaw(DeltaTime);
	UpdateBarrelRecoil(DeltaTime);
}

void UVehicleMovementComponentTank::SyncFromPhysics()
{
	UPhysXVehicleMovementComponent::SyncFromPhysics();

	if (!VehicleInstance)
	{
		return;
	}

	const physx::PxVehicleWheelQueryResult* WheelQueryResult = nullptr;
	UWorld* World = GetWorld();
	FPhysicsScene* PhysicsScene = World ? World->GetPhysicsScene() : nullptr;
	FPhysXVehicleManager* VehicleManager = PhysicsScene ? PhysicsScene->GetVehicleManager() : nullptr;
	if (VehicleManager)
	{
		WheelQueryResult = VehicleManager->GetWheelQueryResult(VehicleInstance->GetPxVehicle());
	}

	VehicleInstance->UpdateVisualState(WheelQueryResult);
	ApplyWheelVisuals();
	ApplyTurretVisual();
	ApplyBarrelVisual();
}

float UVehicleMovementComponentTank::GetLeftTrackSpeed() const
{
	return VehicleInstance ? VehicleInstance->GetLeftTrackSpeed() : 0.0f;
}

float UVehicleMovementComponentTank::GetRightTrackSpeed() const
{
	return VehicleInstance ? VehicleInstance->GetRightTrackSpeed() : 0.0f;
}

float UVehicleMovementComponentTank::GetWheelRotationAngle(const int32 WheelIndex) const
{
	return VehicleInstance && WheelIndex >= 0 ? VehicleInstance->GetWheelRotationAngle(static_cast<uint32>(WheelIndex)) : 0.0f;
}

float UVehicleMovementComponentTank::GetWheelRotationSpeed(const int32 WheelIndex) const
{
	return VehicleInstance && WheelIndex >= 0 ? VehicleInstance->GetWheelRotationSpeed(static_cast<uint32>(WheelIndex)) : 0.0f;
}

int32 UVehicleMovementComponentTank::GetWheelCount() const
{
	return VehicleInstance ? static_cast<int32>(VehicleInstance->GetWheelCount()) : 0;
}

bool UVehicleMovementComponentTank::CreateVehicleInstance(physx::PxPhysics* Physics, physx::PxScene* Scene,
	physx::PxMaterial* Material, const physx::PxTransform& StartPose)
{
	DestroyVehicleInstance();

	VehicleInstance = new FPhysXVehicleTankInstance();
	if (!VehicleInstance->Initialize(Physics, Scene, Material, StartPose, TankSetup))
	{
		DestroyVehicleInstance();
		return false;
	}

	RebuildVisualBindings();
	return true;
}

void UVehicleMovementComponentTank::DestroyVehicleInstance()
{
	ClearVisualBindings();

	if (VehicleInstance)
	{
		VehicleInstance->Shutdown();
		delete VehicleInstance;
		VehicleInstance = nullptr;
	}
}

physx::PxVehicleWheels* UVehicleMovementComponentTank::GetPxVehicle() const
{
	return VehicleInstance ? VehicleInstance->GetPxVehicle() : nullptr;
}

physx::PxRigidDynamic* UVehicleMovementComponentTank::GetVehicleActor() const
{
	return VehicleInstance ? VehicleInstance->GetActor() : nullptr;
}

void UVehicleMovementComponentTank::RebuildVisualBindings()
{
	ClearVisualBindings();

	if (VisualSetup.bAutoBindWheelVisuals && VehicleInstance)
	{
		const uint32 WheelCount = VehicleInstance->GetWheelCount();
		const uint32 WheelPairs = WheelCount / 2;

		for (uint32 PairIndex = 0; PairIndex < WheelPairs; ++PairIndex)
		{
			BindWheelVisualByName(MakeWheelVisualName(VisualSetup.LeftWheelNamePrefix, PairIndex), PairIndex * 2);
			BindWheelVisualByName(MakeWheelVisualName(VisualSetup.RightWheelNamePrefix, PairIndex), PairIndex * 2 + 1);
		}
	}

	if (VisualSetup.bAutoBindTurretVisual)
	{
		BindTurretVisualByName(VisualSetup.TurretComponentName);
	}

	if (VisualSetup.bAutoBindBarrelVisual)
	{
		BindBarrelVisualByName(VisualSetup.BarrelComponentName);
	}
}

void UVehicleMovementComponentTank::BindWheelVisualByName(const FString& ComponentName, const uint32 WheelIndex)
{
	if (ComponentName.empty())
	{
		return;
	}

	USceneComponent* Component = FindOwnerSceneComponentByName(ComponentName);
	if (!Component)
	{
		return;
	}

	FWheelVisualBinding Binding;
	Binding.Component = Component;
	Binding.InitialRelativeLocation = Component->GetRelativeLocation();
	Binding.InitialRelativeRotation = Component->GetRelativeQuat();
	Binding.WheelIndex = WheelIndex;
	WheelVisualBindings.push_back(Binding);
}

void UVehicleMovementComponentTank::BindTurretVisualByName(const FString& ComponentName)
{
	if (ComponentName.empty())
	{
		return;
	}

	USceneComponent* Component = FindOwnerSceneComponentByName(ComponentName);
	if (!Component)
	{
		return;
	}

	TurretVisualComponent = Component;
	InitialTurretRelativeRotation = Component->GetRelativeQuat();
	ApplyTurretVisual();
}

void UVehicleMovementComponentTank::BindBarrelVisualByName(const FString& ComponentName)
{
	if (ComponentName.empty())
	{
		return;
	}

	USceneComponent* Component = FindOwnerSceneComponentByName(ComponentName);
	if (!Component)
	{
		return;
	}

	BarrelVisualComponent = Component;
	InitialBarrelRelativeLocation = Component->GetRelativeLocation();
	ApplyBarrelVisual();
}

USceneComponent* UVehicleMovementComponentTank::FindOwnerSceneComponentByName(const FString& ComponentName) const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || ComponentName.empty())
	{
		return nullptr;
	}

	const FName TargetName(ComponentName);
	for (UActorComponent* Component : OwnerActor->GetComponents())
	{
		USceneComponent* SceneComponent = Cast<USceneComponent>(Component);
		if (SceneComponent && SceneComponent->GetFName() == TargetName)
		{
			return SceneComponent;
		}
	}

	return nullptr;
}

void UVehicleMovementComponentTank::ApplyWheelVisuals()
{
	if (!VehicleInstance || !VisualSetup.bAutoBindWheelVisuals || WheelVisualBindings.empty())
	{
		return;
	}

	FVector RollAxis = VisualSetup.WheelRollAxis;
	if (RollAxis.IsNearlyZero())
	{
		RollAxis = FVector::YAxisVector;
	}
	else
	{
		RollAxis.Normalize();
	}

	for (const FWheelVisualBinding& Binding : WheelVisualBindings)
	{
		if (!IsAliveObject(Binding.Component))
		{
			continue;
		}

		const float SuspensionOffset = VehicleInstance->GetWheelSuspensionOffset(Binding.WheelIndex) * VisualSetup.SuspensionScale;
		const FVector RelativeLocation = Binding.InitialRelativeLocation + FVector(0.0f, 0.0f, SuspensionOffset);

		const bool bLeftWheel = (Binding.WheelIndex % 2) == 0;
		const float RotationScale = bLeftWheel ? VisualSetup.LeftWheelRotationScale : VisualSetup.RightWheelRotationScale;
		const float RotationAngle = VehicleInstance->GetWheelRotationAngle(Binding.WheelIndex) * RotationScale;
		const FQuat RollRotation = FQuat::FromAxisAngle(RollAxis, RotationAngle);

		Binding.Component->SetRelativeLocation(RelativeLocation);
		Binding.Component->SetRelativeRotation((Binding.InitialRelativeRotation * RollRotation).GetNormalized());
	}
}

void UVehicleMovementComponentTank::ApplyTurretVisual()
{
	if (!VisualSetup.bAutoBindTurretVisual || !IsAliveObject(TurretVisualComponent))
	{
		return;
	}

	TurretVisualComponent->SetRelativeRotation((InitialTurretRelativeRotation * GetTurretYawRotation()).GetNormalized());
}

void UVehicleMovementComponentTank::ApplyBarrelVisual()
{
	if (!VisualSetup.bAutoBindBarrelVisual || !IsAliveObject(BarrelVisualComponent))
	{
		return;
	}

	const FVector RelativeLocation = InitialBarrelRelativeLocation + GetBarrelRecoilAxis() * GetBarrelRecoilOffset();
	BarrelVisualComponent->SetRelativeLocation(RelativeLocation);
}

void UVehicleMovementComponentTank::ClearVisualBindings()
{
	if (IsAliveObject(BarrelVisualComponent))
	{
		BarrelVisualComponent->SetRelativeLocation(InitialBarrelRelativeLocation);
	}

	WheelVisualBindings.clear();
	TurretVisualComponent = nullptr;
	InitialTurretRelativeRotation = FQuat::Identity;
	BarrelVisualComponent = nullptr;
	InitialBarrelRelativeLocation = FVector::ZeroVector;
	TurretYawInput = 0.0f;
	BarrelRecoilElapsed = 0.0f;
	bBarrelRecoilActive = false;
}

void UVehicleMovementComponentTank::UpdateTurretYaw(const float DeltaTime)
{
	if (std::abs(TurretYawInput) <= FMath::Epsilon)
	{
		return;
	}

	TurretYawDegrees = ClampTurretYaw(TurretYawDegrees + TurretYawInput * VisualSetup.TurretYawSpeedDegPerSecond * DeltaTime);
}

void UVehicleMovementComponentTank::UpdateBarrelRecoil(const float DeltaTime)
{
	if (!bBarrelRecoilActive)
	{
		return;
	}

	const float KickTime = std::max(MainGunSetup.BarrelKickTime, 0.0f);
	const float ReturnTime = std::max(MainGunSetup.BarrelReturnTime, 0.0f);
	const float TotalTime = KickTime + ReturnTime;
	if (TotalTime <= FMath::Epsilon)
	{
		bBarrelRecoilActive = false;
		BarrelRecoilElapsed = 0.0f;
		ApplyBarrelVisual();
		return;
	}

	BarrelRecoilElapsed += std::max(DeltaTime, 0.0f);
	if (BarrelRecoilElapsed >= TotalTime)
	{
		bBarrelRecoilActive = false;
		BarrelRecoilElapsed = 0.0f;
	}
}

float UVehicleMovementComponentTank::ClampTurretYaw(const float YawDegrees) const
{
	if (!VisualSetup.bLimitTurretYaw)
	{
		return YawDegrees;
	}

	const float MinYaw = std::min(VisualSetup.MinTurretYawDegrees, VisualSetup.MaxTurretYawDegrees);
	const float MaxYaw = std::max(VisualSetup.MinTurretYawDegrees, VisualSetup.MaxTurretYawDegrees);
	return FMath::Clamp(YawDegrees, MinYaw, MaxYaw);
}

FQuat UVehicleMovementComponentTank::GetTurretYawRotation() const
{
	return FQuat::FromAxisAngle(GetTurretYawAxis(), TurretYawDegrees * DEG_TO_RAD);
}

FVector UVehicleMovementComponentTank::GetTurretYawAxis() const
{
	FVector Axis = VisualSetup.TurretYawAxis;
	if (Axis.IsNearlyZero())
	{
		Axis = FVector::UpVector;
	}
	else
	{
		Axis.Normalize();
	}

	return Axis;
}

float UVehicleMovementComponentTank::GetBarrelRecoilOffset() const
{
	const float Distance = std::max(MainGunSetup.BarrelRecoilDistance, 0.0f);
	if (!bBarrelRecoilActive || Distance <= FMath::Epsilon)
	{
		return 0.0f;
	}

	const float KickTime = std::max(MainGunSetup.BarrelKickTime, 0.0f);
	const float ReturnTime = std::max(MainGunSetup.BarrelReturnTime, 0.0f);

	if (KickTime <= FMath::Epsilon)
	{
		if (ReturnTime <= FMath::Epsilon)
		{
			return 0.0f;
		}

		const float ReturnAlpha = SmoothStep(BarrelRecoilElapsed / ReturnTime);
		return Distance * (1.0f - ReturnAlpha);
	}

	if (BarrelRecoilElapsed < KickTime)
	{
		return Distance * SmoothStep(BarrelRecoilElapsed / KickTime);
	}

	if (ReturnTime <= FMath::Epsilon)
	{
		return 0.0f;
	}

	const float ReturnAlpha = SmoothStep((BarrelRecoilElapsed - KickTime) / ReturnTime);
	return Distance * (1.0f - ReturnAlpha);
}

FVector UVehicleMovementComponentTank::GetBarrelRecoilAxis() const
{
	FVector Axis = VisualSetup.BarrelRecoilAxis;
	if (Axis.IsNearlyZero())
	{
		Axis = FVector::BackwardVector;
	}
	else
	{
		Axis.Normalize();
	}

	return Axis;
}

bool UVehicleMovementComponentTank::GetTurretLocalFireTransform(const FVector TurretLocalPoint,
	const FVector TurretLocalDirection, FVector& OutVehicleLocalPoint, FVector& OutVehicleLocalDirection) const
{
	if (TurretLocalDirection.IsNearlyZero())
	{
		return false;
	}

	AActor* OwnerActor = GetOwner();
	USceneComponent* RootComponent = OwnerActor ? OwnerActor->GetRootComponent() : nullptr;

	if (IsAliveObject(TurretVisualComponent) && RootComponent)
	{
		const FMatrix& TurretWorldMatrix = TurretVisualComponent->GetWorldMatrix();
		const FMatrix& VehicleWorldInverseMatrix = RootComponent->GetWorldInverseMatrix();

		const FVector WorldFirePoint = TurretWorldMatrix.TransformPositionWithW(TurretLocalPoint);
		const FVector WorldDirection = TurretWorldMatrix.TransformVector(TurretLocalDirection);

		OutVehicleLocalPoint = VehicleWorldInverseMatrix.TransformPositionWithW(WorldFirePoint);
		OutVehicleLocalDirection = VehicleWorldInverseMatrix.TransformVector(WorldDirection);
		OutVehicleLocalDirection.Normalize();
		return true;
	}

	const FQuat TurretRotation = GetTurretYawRotation();
	OutVehicleLocalPoint = TurretRotation.RotateVector(TurretLocalPoint);
	OutVehicleLocalDirection = TurretRotation.RotateVector(TurretLocalDirection);
	OutVehicleLocalDirection.Normalize();
	return true;
}
