#include "GameFramework/PlayerController.h"

#include "Component/CameraComponent.h"
#include "Component/SpringArmComponent.h"
#include "Core/Logging/Log.h"
#include "GameFramework/PrimitiveActors.h"
#include "GameFramework/World.h"

#include <cmath>
#include <windows.h>

DEFINE_CLASS(APlayerController, AActor)
REGISTER_FACTORY(APlayerController)

namespace
{
	FVector GetPlanarForwardFromCamera(UCameraComponent* Camera)
	{
		if (!Camera)
		{
			return FVector(1.0f, 0.0f, 0.0f);
		}

		FVector Forward = Camera->GetWorldTransform().GetUnitAxis(EAxis::X);
		Forward.Z = 0.0f;
		if (Forward.IsNearlyZero())
		{
			return FVector(1.0f, 0.0f, 0.0f);
		}
		return Forward.GetSafeNormal();
	}

	FVector GetPlanarRightFromCamera(UCameraComponent* Camera)
	{
		if (!Camera)
		{
			return FVector(0.0f, 1.0f, 0.0f);
		}

		FVector Right = Camera->GetWorldTransform().GetUnitAxis(EAxis::Y);
		Right.Z = 0.0f;
		if (Right.IsNearlyZero())
		{
			return FVector(0.0f, 1.0f, 0.0f);
		}
		return Right.GetSafeNormal();
	}

	FQuat MakeViewQuatFromCamera(UCameraComponent* Camera)
	{
		if (!Camera)
		{
			return FQuat::Identity;
		}

		FQuat Result = Camera->GetWorldTransform().GetRotation();
		Result.Normalize();
		return Result;
	}
}

void APlayerController::BeginPlay()
{
	AActor::BeginPlay();

	if (!PossessedActor)
	{
		SpawnDefaultPawn();
	}
	UpdateRuntimeCameraFromViewTarget();
	if (UWorld* World = GetFocusedWorld())
	{
		World->SetActiveCamera(&RuntimeCamera);
	}
	UE_LOG("[PlayerController] BeginPlay. Possessed=%s Camera=%s",
		PossessedActor ? PossessedActor->GetFName().ToString().c_str() : "None",
		ViewTargetCamera ? ViewTargetCamera->GetName().c_str() : "None");
}

void APlayerController::Tick(float DeltaTime)
{
	AActor::Tick(DeltaTime);

	UpdatePossessedActorMovement(DeltaTime);
	UpdateRuntimeCameraFromViewTarget();
	PendingMoveInput = FVector::ZeroVector;
}

void APlayerController::ConfigureRuntimeCameraFromViewport(const FViewportCamera* SourceCamera)
{
	if (!SourceCamera)
	{
		return;
	}

	RuntimeCamera.OnResize(SourceCamera->GetWidth(), SourceCamera->GetHeight());
	RuntimeCamera.SetFOV(SourceCamera->GetFOV());
	RuntimeCamera.SetNearPlane(SourceCamera->GetNearPlane());
	RuntimeCamera.SetFarPlane(SourceCamera->GetFarPlane());
	RuntimeCamera.SetProjectionType(EViewportProjectionType::Perspective);
}

AActor* APlayerController::SpawnDefaultPawn()
{
	UWorld* World = GetFocusedWorld();
	if (!World)
	{
		return nullptr;
	}

	FVector SpawnLocation = FVector::ZeroVector;
	FVector SpawnRotation = FVector::ZeroVector;
	if (AActor* Start = FindPlayerStart())
	{
		SpawnLocation = Start->GetActorLocation();
		SpawnRotation = Start->GetActorRotation();
	}

	ADefaultPlayerActor* Pawn = World->SpawnActor<ADefaultPlayerActor>();
	if (!Pawn)
	{
		return nullptr;
	}

	Pawn->InitDefaultComponents();
	Pawn->SetFName(FName("Default Player"));
	ApplyInitialPawnTransform(Pawn, SpawnLocation, SpawnRotation);

	Possess(Pawn);
	SetViewTarget(Pawn);
	UE_LOG("[PlayerController] Spawned default pawn at (%.2f, %.2f, %.2f).",
		SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z);
	return Pawn;
}

void APlayerController::ApplyInitialPawnTransform(ADefaultPlayerActor* Pawn, const FVector& SpawnLocation, const FVector& SpawnRotation)
{
	if (!Pawn)
	{
		return;
	}

	Pawn->SetActorLocation(SpawnLocation);
	if (USpringArmComponent* SpringArm = Pawn->GetSpringArmComponent())
	{
		SpringArm->SetRelativeRotation(SpawnRotation);
	}
	else if (UCameraComponent* PawnCamera = Pawn->GetCameraComponent())
	{
		PawnCamera->SetRelativeRotation(SpawnRotation);
	}
}

void APlayerController::Possess(AActor* InActor)
{
	PossessedActor = InActor;
	if (PossessedActor)
	{
		SetViewTarget(PossessedActor);
	}
	OnPossess(PossessedActor);
}

void APlayerController::UnPossess()
{
	AActor* OldActor = PossessedActor;
	PossessedActor = nullptr;
	OnUnPossess(OldActor);
}

void APlayerController::SetViewTarget(AActor* InActor)
{
	ViewTargetActor = InActor;
	SetViewTargetCamera(FindCameraComponent(InActor));
}

void APlayerController::SetViewTargetCamera(UCameraComponent* InCamera)
{
	ViewTargetCamera = InCamera;
	UpdateRuntimeCameraFromViewTarget();
}

void APlayerController::HandleKeyPressed(int VK)
{
	(void)VK;
}

void APlayerController::HandleKeyDown(int VK)
{
	switch (VK)
	{
	case 'W':
		AddMoveInput(1.0f, 0.0f);
		break;
	case 'S':
		AddMoveInput(-1.0f, 0.0f);
		break;
	case 'D':
		AddMoveInput(0.0f, 1.0f);
		break;
	case 'A':
		AddMoveInput(0.0f, -1.0f);
		break;
	case 'E':
		AddMoveInput(0.0f, 0.0f, 1.0f);
		break;
	case 'Q':
		AddMoveInput(0.0f, 0.0f, -1.0f);
		break;
	default:
		break;
	}
}

void APlayerController::HandleKeyReleased(int VK)
{
	(void)VK;
}

void APlayerController::HandleMouseMove(float DeltaX, float DeltaY)
{
	if (!ViewTargetCamera)
	{
		return;
	}

	if (USpringArmComponent* SpringArm = Cast<USpringArmComponent>(ViewTargetCamera->GetParent()))
	{
		SpringArm->AddYawInput(DeltaX * LookSensitivity);
		SpringArm->AddPitchInput(-DeltaY * LookSensitivity);
		return;
	}

	ViewTargetCamera->AddYawInput(DeltaX * LookSensitivity);
	ViewTargetCamera->AddPitchInput(-DeltaY * LookSensitivity);
}

UCameraComponent* APlayerController::FindCameraComponent(AActor* Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}

	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (UCameraComponent* Camera = Cast<UCameraComponent>(Component))
		{
			return Camera;
		}
	}
	return nullptr;
}

AActor* APlayerController::FindPlayerStart() const
{
	const UWorld* World = GetFocusedWorld();
	if (!World)
	{
		return nullptr;
	}

	for (AActor* Actor : World->GetActors())
	{
		if (Actor && Actor->IsA<APlayerStart>())
		{
			return Actor;
		}
	}
	return nullptr;
}

void APlayerController::AddMoveInput(float ForwardScale, float RightScale, float UpScale)
{
	PendingMoveInput.X += ForwardScale;
	PendingMoveInput.Y += RightScale;
	PendingMoveInput.Z += UpScale;
}

void APlayerController::UpdatePossessedActorMovement(float DeltaTime)
{
	if (!PossessedActor || PendingMoveInput.IsNearlyZero())
	{
		return;
	}

	FVector MoveDirection =
		GetPlanarForwardFromCamera(ViewTargetCamera) * PendingMoveInput.X
		+ GetPlanarRightFromCamera(ViewTargetCamera) * PendingMoveInput.Y
		+ FVector::UpVector * PendingMoveInput.Z;

	if (MoveDirection.IsNearlyZero())
	{
		return;
	}

	MoveDirection = MoveDirection.GetSafeNormal();
	PossessedActor->AddActorWorldOffset(MoveDirection * MoveSpeed * DeltaTime);
}

void APlayerController::UpdateRuntimeCameraFromViewTarget()
{
	if (!ViewTargetCamera)
	{
		return;
	}

	RuntimeCamera.SetProjectionType(ViewTargetCamera->IsOrthogonal()
		? EViewportProjectionType::Orthographic
		: EViewportProjectionType::Perspective);
	RuntimeCamera.SetLocation(ViewTargetCamera->GetWorldLocation());
	RuntimeCamera.SetRotation(MakeViewQuatFromCamera(ViewTargetCamera));
	RuntimeCamera.SetFOV(ViewTargetCamera->GetFOV());
	RuntimeCamera.SetNearPlane(ViewTargetCamera->GetNearPlane());
	RuntimeCamera.SetFarPlane(ViewTargetCamera->GetFarPlane());
	RuntimeCamera.SetOrthoHeight(ViewTargetCamera->GetOrthoWidth());
}

void APlayerController::OnPossess(AActor* InActor)
{
	(void)InActor;
}

void APlayerController::OnUnPossess(AActor* OldActor)
{
	(void)OldActor;
}
