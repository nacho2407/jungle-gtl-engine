#include "GameFramework/PlayerController.h"

#include "Component/CameraComponent.h"
#include "Component/SpringArmComponent.h"
#include "Core/Logging/Log.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/PrimitiveActors.h"
#include "GameFramework/World.h"
#include "Math/Utils.h"
#include "Camera/PlayerCameraManager.h"

#include <algorithm>
#include <cmath>

DEFINE_CLASS(APlayerController, AActor)
REGISTER_FACTORY(APlayerController)

namespace
{
    constexpr float FallbackCameraMoveSpeed = 10.0f;
    constexpr float FallbackCameraMouseSensitivity = 0.15f;

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

	SpawnPlayerCameraManager();

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
			UE_LOG("[PlayerController] Possessed placed player actor: %s",
				PlacedPlayer->GetFName().ToString().c_str());
		}
		else
		{
			if (AFallbackCameraActor* FallbackCamera = SpawnFallbackCameraActor())
			{
				Possess(FallbackCamera);
				UE_LOG_WARNING("[PlayerController] Player actor with tag 'Player' is missing. Fallback camera actor was created.");
			}
			else
			{
				UE_LOG_ERROR("[PlayerController] Player actor with tag 'Player' is missing and fallback camera creation failed.");
			}
		}
	}
	UpdateRuntimeCameraFromViewTarget();
	if (UWorld* World = GetFocusedWorld())
	{
		World->SetActiveCamera(&RuntimeCamera);
	}
	UE_LOG("[PlayerController] BeginPlay. Possessed=%s",
		PossessedActor ? PossessedActor->GetFName().ToString().c_str() : "None");
}

void APlayerController::Tick(float DeltaTime)
{
	AActor::Tick(DeltaTime);

	LastInputDeltaTime = std::max(0.0f, DeltaTime);
	UpdatePossessedActorMovement(DeltaTime);
	UpdateRuntimeCameraFromViewTarget(DeltaTime);
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
	UE_LOG("[PlayerController] Spawned default pawn at (%.2f, %.2f, %.2f).",
		SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z);
	return Pawn;
}

AFallbackCameraActor* APlayerController::SpawnFallbackCameraActor()
{
	UWorld* World = GetFocusedWorld();
	if (!World)
	{
		return nullptr;
	}

	AFallbackCameraActor* CameraActor = World->SpawnActor<AFallbackCameraActor>();
	if (!CameraActor)
	{
		return nullptr;
	}

	CameraActor->InitDefaultComponents();
	CameraActor->SetFName(FName("Fallback Camera"));
	ApplyFallbackCameraTransform(CameraActor);
	return CameraActor;
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

void APlayerController::ApplyFallbackCameraTransform(AFallbackCameraActor* CameraActor)
{
	if (!CameraActor)
	{
		return;
	}

	FVector SpawnLocation = RuntimeCamera.GetLocation();
	FVector SpawnRotation = RuntimeCamera.GetRotation().Euler();
	if (AActor* Start = FindPlayerStart())
	{
		SpawnLocation = Start->GetActorLocation();
		SpawnRotation = Start->GetActorRotation();
	}

	CameraActor->SetActorLocation(SpawnLocation);
	if (UCameraComponent* Camera = CameraActor->GetCameraComponent())
	{
		Camera->SetRelativeRotation(FVector(0.0f, SpawnRotation.Y, SpawnRotation.Z));
		FCameraState CameraState = Camera->GetCameraState();
		CameraState.FOV = RuntimeCamera.GetFOV();
		CameraState.NearZ = RuntimeCamera.GetNearPlane();
		CameraState.FarZ = RuntimeCamera.GetFarPlane();
		CameraState.bIsOrthogonal = RuntimeCamera.IsOrthographic();
		CameraState.OrthoWidth = RuntimeCamera.GetOrthoHeight();
		Camera->SetCameraState(CameraState);
	}
}

void APlayerController::Possess(AActor* InActor)
{
	PossessedActor = InActor;
	OnPossess(PossessedActor);
}

void APlayerController::UnPossess()
{
	AActor* OldActor = PossessedActor;
	PossessedActor = nullptr;
	OnUnPossess(OldActor);
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

void APlayerController::SetMouseCapture(bool bCaptured)
{
	if (bCaptured)
	{
		SetInputModeGameOnly();
	}
	else
	{
		SetInputModeGameAndUI();
	}
}

void APlayerController::ReleaseMouseCapture()
{
	SetMouseCapture(false);
}

bool APlayerController::IsMouseCaptured() const
{
	return GEngine ? GEngine->IsRuntimeCursorLocked() && !GEngine->IsRuntimeCursorVisible() : false;
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

void APlayerController::PlayCameraShake(float Intensity, float Duration)
{
	const float SafeIntensity = std::max(0.0f, Intensity);
	PlayCameraShakeDetailed(SafeIntensity * 0.08f, SafeIntensity * 3.0f, 18.0f, Duration);
}

void APlayerController::PlayCameraShakeDetailed(float LocationAmplitude, float RotationAmplitudeDegrees, float Frequency, float Duration)
{
	CameraShake.bActive = Duration > 0.0f && (LocationAmplitude > 0.0f || RotationAmplitudeDegrees > 0.0f);
	CameraShake.Elapsed = 0.0f;
	CameraShake.Duration = std::max(0.0f, Duration);
	CameraShake.Frequency = std::max(0.1f, Frequency);
	CameraShake.LocationAmplitude = std::max(0.0f, LocationAmplitude);
	CameraShake.RotationAmplitudeDegrees = std::max(0.0f, RotationAmplitudeDegrees);
	CameraShake.Seed += 1.0f;
	CameraShake.PhaseX = CameraShake.Seed * 1.37f;
	CameraShake.PhaseY = CameraShake.Seed * 2.11f + 1.7f;
	CameraShake.PhaseZ = CameraShake.Seed * 2.89f + 3.1f;
}

void APlayerController::LerpCameraFOVDegrees(float TargetFOVDegrees, float Duration)
{
	CameraFOV.bActive = true;
	CameraFOV.bResetToBase = false;
	CameraFOV.Elapsed = 0.0f;
	CameraFOV.Duration = std::max(0.0f, Duration);
	CameraFOV.StartFOV = RuntimeCamera.GetFOV();
	CameraFOV.TargetFOV = MathUtil::DegreesToRadians(MathUtil::Clamp(TargetFOVDegrees, 1.0f, 179.0f));
	CameraFOV.OverrideFOV = CameraFOV.TargetFOV;
	CameraFOV.bOverrideActive = true;
}

void APlayerController::ResetCameraFOV(float Duration)
{
	CameraFOV.bActive = true;
	CameraFOV.bResetToBase = true;
	CameraFOV.Elapsed = 0.0f;
	CameraFOV.Duration = std::max(0.0f, Duration);
	CameraFOV.StartFOV = RuntimeCamera.GetFOV();

	UCameraComponent* Camera = GetViewTargetCamera();
    CameraFOV.TargetFOV = Camera ? Camera->GetFOV() : RuntimeCamera.GetFOV();
}

void APlayerController::StopCameraEffects()
{
	CameraShake.bActive = false;
	CameraFOV.bActive = false;
	CameraFOV.bOverrideActive = false;
	if (PlayerCameraManager)
	{
		PlayerCameraManager->StopAllCameraShakes(true);
	}
}

void APlayerController::HandleKeyPressed(int VK)
{
	(void)VK;
}

void APlayerController::HandleKeyDown(int VK)
{
	if (IsPossessingFallbackCamera())
	{
		MoveFallbackCamera(VK, LastInputDeltaTime);
		return;
	}

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
	if (IsPossessingFallbackCamera())
	{
		if (UCameraComponent* Camera = GetViewTargetCamera())
		{
			Camera->AddYawInput(DeltaX * FallbackCameraMouseSensitivity);
			Camera->AddPitchInput(-DeltaY * FallbackCameraMouseSensitivity);
		}
		return;
	}

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

AActor* APlayerController::GetViewTargetActor() const
{
    return PlayerCameraManager ? PlayerCameraManager->GetViewTargetActor() : nullptr;
}

UCameraComponent* APlayerController::GetViewTargetCamera() const
{
    return PlayerCameraManager ? PlayerCameraManager->GetViewTargetCamera() : nullptr;
}

void APlayerController::SpawnPlayerCameraManager()
{
	if (PlayerCameraManager)
	{
		return;
	}

	PlayerCameraManager = GetFocusedWorld()->SpawnActor<APlayerCameraManager>();
	if (!PlayerCameraManager)
	{
		return;
	}

	PlayerCameraManager->InitializeFor(this);
	if (PendingCameraFade.bPending)
	{
		PlayerCameraManager->StartFade(
			PendingCameraFade.FromAlpha,
			PendingCameraFade.ToAlpha,
			PendingCameraFade.Duration,
			PendingCameraFade.Color);
		PendingCameraFade.bPending = false;
	}
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
	}

	if (FirstTaggedPlayer)
	{
		UE_LOG_WARNING("[PlayerController] Actor tagged Player has no CameraComponent: %s",
			FirstTaggedPlayer->GetFName().ToString().c_str());
	}
	return nullptr;
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
}

void APlayerController::UpdatePossessedActorMovement(float DeltaTime)
{
	(void)DeltaTime;
}

void APlayerController::UpdateCameraFOVEffect(float DeltaTime, float BaseFOV)
{
	if (!CameraFOV.bActive)
	{
		RuntimeCamera.SetFOV(CameraFOV.bOverrideActive ? CameraFOV.OverrideFOV : BaseFOV);
		return;
	}

	CameraFOV.TargetFOV = CameraFOV.bResetToBase ? BaseFOV : CameraFOV.TargetFOV;

	if (CameraFOV.Duration <= 0.0f)
	{
		RuntimeCamera.SetFOV(CameraFOV.TargetFOV);
		if (CameraFOV.bResetToBase)
		{
			CameraFOV.bOverrideActive = false;
		}
		CameraFOV.bActive = false;
		return;
	}

	CameraFOV.Elapsed += std::max(0.0f, DeltaTime);
	float Alpha = MathUtil::Clamp(CameraFOV.Elapsed / CameraFOV.Duration, 0.0f, 1.0f);
	Alpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);
	const float NewFOV = CameraFOV.StartFOV + (CameraFOV.TargetFOV - CameraFOV.StartFOV) * Alpha;
	RuntimeCamera.SetFOV(NewFOV);

	if (CameraFOV.Elapsed >= CameraFOV.Duration)
	{
		if (CameraFOV.bResetToBase)
		{
			CameraFOV.bOverrideActive = false;
		}
		CameraFOV.bActive = false;
	}
}

void APlayerController::ApplyCameraShakeEffect(float DeltaTime)
{
	// PC 에서 직접적으로 Camera 조작하는 거 방지 (해당 코드는 Legacy 임)
    assert(false && "PlayerCameraManager 쪽으로 로직 옮기는 중입니다.");

	if (!CameraShake.bActive)
	{
		return;
	}

	if (CameraShake.Duration <= 0.0f)
	{
		CameraShake.bActive = false;
		return;
	}

	CameraShake.Elapsed += std::max(0.0f, DeltaTime);
	const float NormalizedTime = MathUtil::Clamp(CameraShake.Elapsed / CameraShake.Duration, 0.0f, 1.0f);
	const float Fade = 1.0f - NormalizedTime;
	const float WaveTime = CameraShake.Elapsed * CameraShake.Frequency * MathUtil::TwoPi;

	const float OffsetX = std::sin(WaveTime + CameraShake.PhaseX) * CameraShake.LocationAmplitude * Fade;
	const float OffsetY = std::sin(WaveTime * 1.13f + CameraShake.PhaseY) * CameraShake.LocationAmplitude * Fade;
	const float OffsetZ = std::sin(WaveTime * 1.31f + CameraShake.PhaseZ) * CameraShake.LocationAmplitude * Fade;

	const FVector BaseLocation = RuntimeCamera.GetLocation();
	const FVector ShakeLocation =
		BaseLocation
		+ RuntimeCamera.GetRightVector() * OffsetX
		+ RuntimeCamera.GetForwardVector() * OffsetY
		+ RuntimeCamera.GetUpVector() * OffsetZ;
	RuntimeCamera.SetLocation(ShakeLocation);

	const float Pitch = std::sin(WaveTime * 1.07f + CameraShake.PhaseY) * CameraShake.RotationAmplitudeDegrees * Fade;
	const float Yaw = std::sin(WaveTime * 0.93f + CameraShake.PhaseX) * CameraShake.RotationAmplitudeDegrees * Fade;
	const float Roll = std::sin(WaveTime * 1.21f + CameraShake.PhaseZ) * CameraShake.RotationAmplitudeDegrees * 0.5f * Fade;
	FQuat ShakeRotation = FQuat::MakeFromEuler(FVector(Roll, Pitch, Yaw));
	FQuat NewRotation = RuntimeCamera.GetRotation() * ShakeRotation;
	NewRotation.Normalize();
	RuntimeCamera.SetRotation(NewRotation);

	if (CameraShake.Elapsed >= CameraShake.Duration)
	{
		CameraShake.bActive = false;
	}
}

void APlayerController::SetViewTargetWithBlend(AActor* InActor, float BlendTime, ECameraBlendType BlendType)
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->SetViewTargetWithBlend(InActor, BlendTime, BlendType);
	}
}

void APlayerController::SetDefaultViewTargetBlend(float BlendTime, ECameraBlendType BlendType)
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->SetDefaultViewTargetBlend(BlendTime, BlendType);
	}
}

void APlayerController::StartCameraFade(float FromAlpha, float ToAlpha, float Duration, const FColor& Color)
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->StartFade(FromAlpha, ToAlpha, Duration, Color);
		return;
	}

	PendingCameraFade.FromAlpha = FromAlpha;
	PendingCameraFade.ToAlpha = ToAlpha;
	PendingCameraFade.Duration = Duration;
	PendingCameraFade.Color = Color;
	PendingCameraFade.bPending = true;
}

void APlayerController::StopCameraFade()
{
	PendingCameraFade.bPending = false;
	if (PlayerCameraManager)
	{
		PlayerCameraManager->StopFade();
	}
}

void APlayerController::SetCameraVignette(float Intensity, float Radius, float Smoothness, const FColor& Color)
{
	if (UCameraComponent* Camera = GetViewTargetCamera())
	{
		Camera->SetVignette(Intensity, Radius, Smoothness, Color);
	}
}

void APlayerController::ClearCameraVignette()
{
	if (UCameraComponent* Camera = GetViewTargetCamera())
	{
		Camera->ClearVignette();
	}
}

void APlayerController::StartCameraLetterbox(float TargetAspect, float Duration)
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->StartLetterbox(TargetAspect, Duration);
	}
}

void APlayerController::StopCameraLetterbox(float Duration)
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->StopLetterbox(Duration);
	}
}

void APlayerController::SetCameraLetterbox(float TargetAspect)
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->SetLetterbox(TargetAspect);
	}
}

void APlayerController::ClearCameraLetterbox()
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ClearLetterbox();
	}
}

void APlayerController::UpdateRuntimeCameraFromViewTarget(float DeltaTime)
{
	ClearInvalidViewTarget();

	FMinimalViewInfo ViewInfo = PlayerCameraManager->GetCameraView();

	RuntimeCamera.SetProjectionType(ViewInfo.bIsOrthogonal
		? EViewportProjectionType::Orthographic
		: EViewportProjectionType::Perspective);
	RuntimeCamera.SetLocation(ViewInfo.Location);
    RuntimeCamera.SetRotation(ViewInfo.Rotation);
    UpdateCameraFOVEffect(DeltaTime, ViewInfo.FOV);
    RuntimeCamera.SetNearPlane(ViewInfo.NearZ);
	RuntimeCamera.SetFarPlane(ViewInfo.FarZ);
    RuntimeCamera.SetOrthoHeight(ViewInfo.OrthoWidth);
	
	// ApplyCameraShakeEffect(DeltaTime);
}

void APlayerController::OnPossess(AActor* InActor)
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->SetViewTarget(InActor);
	}

	if (IsPossessingFallbackCamera())
	{
		SetInputModeGameOnly();
	}
}

void APlayerController::OnUnPossess(AActor* OldActor)
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->SetViewTarget(this);
	}
}

bool APlayerController::IsPossessingFallbackCamera() const
{
	return PossessedActor && PossessedActor->IsA<AFallbackCameraActor>();
}

void APlayerController::MoveFallbackCamera(int VK, float DeltaTime)
{
	UCameraComponent* Camera = GetViewTargetCamera();
	if (!Camera)
	{
		return;
	}

	const float Distance = FallbackCameraMoveSpeed * std::max(0.0f, DeltaTime);
	switch (VK)
	{
	case 'W':
		Camera->MoveForward(Distance);
		break;
	case 'S':
		Camera->MoveForward(-Distance);
		break;
	case 'D':
		Camera->MoveRight(Distance);
		break;
	case 'A':
		Camera->MoveRight(-Distance);
		break;
	case 'E':
	case VK_SPACE:
		Camera->MoveUp(Distance);
		break;
	case 'Q':
	case VK_CONTROL:
		Camera->MoveUp(-Distance);
		break;
	default:
		break;
	}
}
