#include "GameFramework/PlayerController.h"

#include "Component/CameraComponent.h"
#include "Core/Logging/Log.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/Engine.h"
#include "Camera/ShakePattern/SinusoidalCameraShakePattern.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PrimitiveActors.h"
#include "GameFramework/World.h"
#include "Math/Utils.h"
#include "Camera/PlayerCameraManager.h"

#include <algorithm>

DEFINE_CLASS(APlayerController, AActor)
REGISTER_FACTORY(APlayerController)

namespace
{
    constexpr float DefaultPawnMoveSpeed = 10.0f;
    constexpr float DefaultPawnMouseSensitivity = 0.15f;

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
	if (PossessedPawn)
	{
		OnPossess(PossessedPawn);
	}

	if (!PossessedPawn)
	{
		if (ADefaultPawn* DefaultPawn = SpawnDefaultPawnFallback())
		{
			Possess(DefaultPawn);
			UE_LOG_WARNING("[PlayerController] No pawn was provided by GameMode. DefaultPawn fallback was created.");
		}
		else
		{
			UE_LOG_ERROR("[PlayerController] No pawn was provided by GameMode and DefaultPawn fallback creation failed.");
		}
	}
	UpdateRuntimeCameraFromViewTarget();
	if (UWorld* World = GetFocusedWorld())
	{
		World->SetActiveCamera(&RuntimeCamera);
	}
	UE_LOG("[PlayerController] BeginPlay. Possessed=%s",
		PossessedPawn ? PossessedPawn->GetFName().ToString().c_str() : "None");
}

void APlayerController::Tick(float DeltaTime)
{
	AActor::Tick(DeltaTime);

	UpdateDefaultPawnFromInput(DeltaTime);
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

ADefaultPawn* APlayerController::SpawnDefaultPawnFallback()
{
	UWorld* World = GetFocusedWorld();
	if (!World)
	{
		return nullptr;
	}

	ADefaultPawn* DefaultPawn = World->SpawnActor<ADefaultPawn>();
	if (!DefaultPawn)
	{
		return nullptr;
	}

	DefaultPawn->InitDefaultComponents();
	DefaultPawn->SetFName(FName("DefaultPawn"));
	ApplyDefaultPawnFallbackTransform(DefaultPawn);
	return DefaultPawn;
}

void APlayerController::ApplyDefaultPawnFallbackTransform(ADefaultPawn* DefaultPawn)
{
	if (!DefaultPawn)
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

	DefaultPawn->SetActorLocation(SpawnLocation);
	if (UCameraComponent* Camera = DefaultPawn->GetCameraComponent())
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

void APlayerController::Possess(APawn* InPawn)
{
	if (PossessedPawn == InPawn)
	{
		return;
	}

	if (PossessedPawn)
	{
		APawn* OldPawn = PossessedPawn;
		PossessedPawn = nullptr;
		OldPawn->UnPossessed();
		OnUnPossess(OldPawn);
	}

	PossessedPawn = InPawn;
	if (PossessedPawn)
	{
		PossessedPawn->PossessedBy(this);
	}
	OnPossess(PossessedPawn);
}

void APlayerController::UnPossess()
{
	APawn* OldPawn = PossessedPawn;
	PossessedPawn = nullptr;
	if (OldPawn)
	{
		OldPawn->UnPossessed();
	}
	OnUnPossess(OldPawn);
}

void APlayerController::NotifyObservedActorDestroyed(AActor* DestroyedActor)
{
	if (!DestroyedActor)
	{
		return;
	}

	bool bCleared = false;
	if (PossessedPawn == DestroyedActor)
	{
		APawn* OldPawn = PossessedPawn;
		PossessedPawn = nullptr;
		if (OldPawn)
		{
			OldPawn->UnPossessed();
		}
		OnUnPossess(OldPawn);
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
	if (!PlayerCameraManager || Duration <= 0.0f)
	{
		return;
	}

	USinusoidalCameraShakePattern* Pattern =
		UObjectManager::Get().CreateObject<USinusoidalCameraShakePattern>();
	if (!Pattern)
	{
		return;
	}

	const float SafeLocationAmplitude = std::max(0.0f, LocationAmplitude);
	const float SafeRotationAmplitude = std::max(0.0f, RotationAmplitudeDegrees);
	const float SafeFrequency = std::max(0.1f, Frequency);
	Pattern->LocationAmplitude = FVector(SafeLocationAmplitude, SafeLocationAmplitude, SafeLocationAmplitude);
	Pattern->LocationFrequency = FVector(SafeFrequency, SafeFrequency * 1.13f, SafeFrequency * 1.31f);
	Pattern->LocationPhase = FVector(0.0f, 1.7f, 3.1f);
	Pattern->RotationAmplitudeDeg = FVector(SafeRotationAmplitude, SafeRotationAmplitude, SafeRotationAmplitude * 0.5f);
	Pattern->RotationFrequency = FVector(SafeFrequency * 1.07f, SafeFrequency * 0.93f, SafeFrequency * 1.21f);
	Pattern->RotationPhase = FVector(1.7f, 0.0f, 3.1f);

	PlayerCameraManager->StartCameraShake(Pattern, 1.0f, Duration);
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
	CameraFOV.bActive = false;
	CameraFOV.bOverrideActive = false;
	if (PlayerCameraManager)
	{
		PlayerCameraManager->StopAllCameraShakes(true);
	}
}

void APlayerController::ProcessInputSnapshot(const FGameplayInputSnapshot& Snapshot)
{
	ReceiveInputSnapshot(Snapshot);
	DispatchInputActionsToPawn();
}

void APlayerController::ReceiveInputSnapshot(const FGameplayInputSnapshot& Snapshot)
{
	CurrentInputSnapshot = Snapshot;
}

const FInputActionState* APlayerController::FindInputAction(const FString& ActionName) const
{
	return CurrentInputSnapshot.FindAction(ActionName);
}

void APlayerController::DispatchInputActionsToPawn()
{
	if (!PossessedPawn)
	{
		return;
	}

	for (const auto& Pair : CurrentInputSnapshot.GetActions())
	{
		const FInputActionState& Action = Pair.second;
		if (Action.TriggerEvent != EInputTriggerEvent::None)
		{
			PossessedPawn->OnInputAction(Action);
		}
	}
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
	if (PossessedPawn && !IsActorInCurrentWorld(PossessedPawn))
	{
		APawn* OldPawn = PossessedPawn;
		PossessedPawn = nullptr;
		OldPawn->UnPossessed();
		OnUnPossess(OldPawn);
	}
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
}

void APlayerController::OnPossess(APawn* InPawn)
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->SetViewTarget(InPawn);
	}

	if (IsPossessingDefaultPawn())
	{
		SetInputModeGameOnly();
	}
}

void APlayerController::OnUnPossess(APawn* OldPawn)
{
	(void)OldPawn;
	if (PlayerCameraManager)
	{
		PlayerCameraManager->SetViewTarget(this);
	}
}

bool APlayerController::IsPossessingDefaultPawn() const
{
	return PossessedPawn && PossessedPawn->IsA<ADefaultPawn>();
}

void APlayerController::UpdateDefaultPawnFromInput(float DeltaTime)
{
	if (!IsPossessingDefaultPawn())
	{
		return;
	}

	UCameraComponent* Camera = GetViewTargetCamera();
	if (!Camera)
	{
		return;
	}

	const float Distance = DefaultPawnMoveSpeed * std::max(0.0f, DeltaTime);
	if (const FInputActionState* Move = FindInputAction("Move"))
	{
		const FVector2 Axis = Move->Value.Axis2D;
		if (Axis.Y != 0.0f)
		{
			Camera->MoveForward(Axis.Y * Distance);
		}
		if (Axis.X != 0.0f)
		{
			Camera->MoveRight(Axis.X * Distance);
		}
	}

	if (const FInputActionState* MoveVertical = FindInputAction("MoveVertical"))
	{
		if (MoveVertical->Value.Axis1D != 0.0f)
		{
			Camera->MoveUp(MoveVertical->Value.Axis1D * Distance);
		}
	}

	if (const FInputActionState* Look = FindInputAction("Look"))
	{
		const FVector2 Axis = Look->Value.Axis2D;
		if (Axis.X != 0.0f || Axis.Y != 0.0f)
		{
			Camera->AddYawInput(Axis.X * DefaultPawnMouseSensitivity);
			Camera->AddPitchInput(-Axis.Y * DefaultPawnMouseSensitivity);
		}
	}
}
