#include "GameFramework/PlayerController.h"

#include "Component/CameraComponent.h"
#include "Component/SpringArmComponent.h"
#include "Core/Logging/Log.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/PrimitiveActors.h"
#include "GameFramework/World.h"

DEFINE_CLASS(APlayerController, AActor)
REGISTER_FACTORY(APlayerController)

namespace
{
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
		if (AActor* PlacedPlayer = FindPlacedPlayerActor())
		{
			FVector SpawnLocation = PlacedPlayer->GetActorLocation();
			FVector SpawnRotation = PlacedPlayer->GetActorRotation();
			if (AActor* Start = FindPlayerStart())
			{
				SpawnLocation = Start->GetActorLocation();
				SpawnRotation = Start->GetActorRotation();
			}

			ApplyPlayerStartTransform(PlacedPlayer, SpawnLocation, SpawnRotation);
			Possess(PlacedPlayer);
			SetViewTarget(PlacedPlayer);
			UE_LOG("[PlayerController] Possessed placed player actor: %s",
				PlacedPlayer->GetFName().ToString().c_str());
		}
		else
		{
			SpawnDefaultPawn();
		}
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
	Pawn->AddTag("Player");
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

	ApplyPlayerStartTransform(Pawn, SpawnLocation, SpawnRotation);
}

void APlayerController::ApplyPlayerStartTransform(AActor* Pawn, const FVector& SpawnLocation, const FVector& SpawnRotation)
{
	if (!Pawn)
	{
		return;
	}

	Pawn->SetActorLocation(SpawnLocation);
	Pawn->SetActorRotation(FVector(0.0f, 0.0f, SpawnRotation.Z));

	if (ADefaultPlayerActor* DefaultPawn = Cast<ADefaultPlayerActor>(Pawn))
	{
		if (USpringArmComponent* SpringArm = DefaultPawn->GetSpringArmComponent())
		{
			SpringArm->SetRelativeRotation(FVector::ZeroVector);
			SpringArm->UpdateSocketChildren();
		}
	}

	if (UCameraComponent* PawnCamera = FindCameraComponent(Pawn))
	{
		PawnCamera->SetRelativeRotation(FVector(0.0f, SpawnRotation.Y, 0.0f));
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

void APlayerController::NotifyObservedActorDestroyed(AActor* DestroyedActor)
{
	if (!DestroyedActor)
	{
		return;
	}

	bool bCleared = false;
	if (PossessedActor == DestroyedActor)
	{
		AActor* OldActor = PossessedActor;
		PossessedActor = nullptr;
		OnUnPossess(OldActor);
		bCleared = true;
	}

	if (ViewTargetActor == DestroyedActor)
	{
		ViewTargetActor = nullptr;
		ViewTargetCamera = nullptr;
		bCleared = true;
	}

	if (bCleared)
	{
		UE_LOG_WARNING("[PlayerController] Observed actor destroyed. Runtime possession/view target cleared.");
	}
}

void APlayerController::SetCursorVisible(bool bVisible)
{
	if (GEngine)
	{
		GEngine->SetRuntimeCursorVisible(bVisible);
	}
	InputSystem::Get().SetCursorVisibility(bVisible);
}

bool APlayerController::IsCursorVisible() const
{
	return GEngine ? GEngine->IsRuntimeCursorVisible() : false;
}

void APlayerController::SetCursorLocked(bool bLocked)
{
	if (GEngine)
	{
		GEngine->SetRuntimeCursorLocked(bLocked);
	}
	if (!bLocked)
	{
		InputSystem::Get().SetUseRawMouse(false);
		InputSystem::Get().LockMouse(false);
	}
}

bool APlayerController::IsCursorLocked() const
{
	return GEngine ? GEngine->IsRuntimeCursorLocked() : false;
}

void APlayerController::SetInputModeGameOnly()
{
	if (GEngine)
	{
		GEngine->SetRuntimeInputMode(ERuntimeInputMode::GameOnly);
	}
}

void APlayerController::SetInputModeUIOnly()
{
	if (GEngine)
	{
		GEngine->SetRuntimeInputMode(ERuntimeInputMode::UIOnly);
	}
}

void APlayerController::SetInputModeGameAndUI()
{
	if (GEngine)
	{
		GEngine->SetRuntimeInputMode(ERuntimeInputMode::GameAndUI);
	}
}

void APlayerController::HandleKeyPressed(int VK)
{
	(void)VK;
}

void APlayerController::HandleKeyDown(int VK)
{
	// Lua 게임 로직이 Engine.API.Input을 통해 raw input을 직접 읽습니다.
	// 여기서 기본 WASD 이동을 처리하면 Lua 이동과 중복되므로 PlayerController는 입력을 소비하지 않습니다.
	(void)VK;
}

void APlayerController::HandleKeyReleased(int VK)
{
	(void)VK;
}

void APlayerController::HandleMouseMove(float DeltaX, float DeltaY)
{
	// 마우스 회전 역시 Lua 쪽 Player/InputManager가 필요에 따라 처리합니다.
	// PlayerController는 ViewTarget Camera를 RuntimeCamera에 반영하는 것까지만 담당합니다.
	(void)DeltaX;
	(void)DeltaY;
}

void APlayerController::HandleMouseMoveAbsolute(float X, float Y)
{
	(void)X;
	(void)Y;
}

void APlayerController::HandleMouseButtonPressed(int VK, float X, float Y)
{
	(void)VK;
	(void)X;
	(void)Y;
}

void APlayerController::HandleMouseButtonDown(int VK, float DeltaX, float DeltaY)
{
	(void)VK;
	(void)DeltaX;
	(void)DeltaY;
}

void APlayerController::HandleMouseButtonReleased(int VK, float X, float Y)
{
	(void)VK;
	(void)X;
	(void)Y;
}

void APlayerController::HandleMouseDrag(int VK, float DeltaX, float DeltaY)
{
	(void)VK;
	(void)DeltaX;
	(void)DeltaY;
}

void APlayerController::HandleMouseDragEnd(int VK, float X, float Y)
{
	(void)VK;
	(void)X;
	(void)Y;
}

void APlayerController::HandleMouseWheel(float Notch)
{
	(void)Notch;
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

AActor* APlayerController::FindPlacedPlayerActor() const
{
	const UWorld* World = GetFocusedWorld();
	if (!World)
	{
		return nullptr;
	}

	AActor* FirstTaggedPlayer = nullptr;
	AActor* FirstDefaultPlayer = nullptr;
	for (AActor* Actor : World->GetActors())
	{
		if (!Actor || Actor == this || Actor->IsA<APlayerController>() || Actor->IsA<APlayerStart>())
		{
			continue;
		}

		const bool bHasCamera = FindCameraComponent(Actor) != nullptr;
		if (Actor->HasTag("Player"))
		{
			if (!FirstTaggedPlayer)
			{
				FirstTaggedPlayer = Actor;
			}

			if (bHasCamera)
			{
				return Actor;
			}
		}
		else if (!FirstDefaultPlayer && Actor->IsA<ADefaultPlayerActor>() && bHasCamera)
		{
			FirstDefaultPlayer = Actor;
		}
	}

	if (FirstTaggedPlayer)
	{
		UE_LOG_WARNING("[PlayerController] Actor tagged Player has no CameraComponent: %s",
			FirstTaggedPlayer->GetFName().ToString().c_str());
	}
	if (FirstDefaultPlayer)
	{
		FirstDefaultPlayer->AddTag("Player");
		UE_LOG_WARNING("[PlayerController] Reusing default player actor without Player tag: %s",
			FirstDefaultPlayer->GetFName().ToString().c_str());
	}
	return FirstDefaultPlayer;
}

bool APlayerController::IsActorInCurrentWorld(const AActor* Actor) const
{
	const UWorld* World = GetFocusedWorld();
	if (!World || !Actor)
	{
		return false;
	}

	for (AActor* WorldActor : World->GetActors())
	{
		if (WorldActor == Actor)
		{
			return true;
		}
	}
	return false;
}

void APlayerController::ClearInvalidViewTarget()
{
	if (PossessedActor && !IsActorInCurrentWorld(PossessedActor))
	{
		PossessedActor = nullptr;
	}

	if (ViewTargetActor && !IsActorInCurrentWorld(ViewTargetActor))
	{
		ViewTargetActor = nullptr;
		ViewTargetCamera = nullptr;
	}

	if (!ViewTargetActor)
	{
		ViewTargetCamera = nullptr;
	}
}

void APlayerController::UpdatePossessedActorMovement(float DeltaTime)
{
	(void)DeltaTime;
}

void APlayerController::UpdateRuntimeCameraFromViewTarget()
{
	ClearInvalidViewTarget();
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
