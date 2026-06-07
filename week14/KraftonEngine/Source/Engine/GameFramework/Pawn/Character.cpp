#include "GameFramework/Pawn/Character.h"

#include "Component/Camera/CameraComponent.h"
#include "Component/Shape/CapsuleComponent.h"
#include "Component/Input/InputComponent.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Input/InputSystem.h"
#include "Lua/LuaScriptManager.h"
#include "Math/Rotator.h"
#include "Mesh/MeshManager.h"
#include "Runtime/Engine.h"
#include "Viewport/GameViewportClient.h"

#include <algorithm>
#include <cmath>

namespace
{
	bool IsCrouchKeyDown(const InputSystem& Input)
	{
		return Input.GetKey(VK_CONTROL) || Input.GetKey(VK_LCONTROL) || Input.GetKey(VK_RCONTROL);
	}
}

void ACharacter::InitDefaultComponents(const FString& SkeletalMeshFileName)
{
	// 1) Capsule — Root. CharacterMovement 의 UpdatedComponent 가 이걸 가리킴.
	CapsuleComponent = AddComponent<UCapsuleComponent>();
	SetRootComponent(CapsuleComponent);

	// 2) SkeletalMesh — Capsule 의 자식.
	Mesh = AddComponent<USkeletalMeshComponent>();
	Mesh->AttachToComponent(CapsuleComponent);

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	if (!SkeletalMeshFileName.empty())
	{
		USkeletalMesh* Asset = FMeshManager::LoadSkeletalMesh(SkeletalMeshFileName, Device);
		Mesh->SetSkeletalMesh(Asset);
	}

	// 3) CharacterMovement — non-scene. UpdatedComponent = Capsule.
	CharacterMovement = AddComponent<UCharacterMovementComponent>();
	CharacterMovement->SetUpdatedComponent(CapsuleComponent);
}

void ACharacter::PostDuplicate()
{
	Super::PostDuplicate();
	// 컴포넌트 트리 재발견 — Duplicate 후 멤버 포인터 복원.
	CapsuleComponent  = Cast<UCapsuleComponent>(GetRootComponent());
	Mesh              = GetComponentByClass<USkeletalMeshComponent>();
	CharacterMovement = GetComponentByClass<UCharacterMovementComponent>();
	CachedCameraForTilt  = nullptr; // 다음 Tick 에 lazy 재탐색.
	CurrentCameraTiltDeg = 0.0f;
}

void ACharacter::AddMovementInput(const FVector& WorldDirection, float ScaleValue)
{
	if (CharacterMovement)
	{
		const InputSystem& Input = InputSystem::Get();
		CharacterMovement->SetCrouching(IsCrouchKeyDown(Input));
		CharacterMovement->SetSprinting(Input.GetKey(VK_SHIFT));
		CharacterMovement->AddInputVector(WorldDirection, ScaleValue);
	}
}

void ACharacter::Jump()
{
	if (CharacterMovement)
	{
		CharacterMovement->Jump();
	}
}

void ACharacter::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!bAutoInputWASD || !InputComponent) return;

	// Capsule (RootComponent) 기준 — yaw 회전이 곧 캐릭터 facing. mouse look 이 yaw 만
	// 변경 → forward/right vector 가 자동 회전 → WASD 가 "카메라 보는 방향" 으로 이동.
	InputComponent->AddAxisMapping("MoveForward", 'W',  1.0f);
	InputComponent->AddAxisMapping("MoveForward", 'S', -1.0f);
	InputComponent->AddAxisMapping("MoveRight",   'D',  1.0f);
	InputComponent->AddAxisMapping("MoveRight",   'A', -1.0f);

	// WASD 의 forward/right 는 ControlRotation.Yaw 기준 — capsule rotation 과 무관.
	// "카메라가 보는 방향" (yaw 만, pitch 무시) 으로 이동.
	InputComponent->BindAxis("MoveForward", [this](float Value)
	{
		if (Value == 0.0f) return;
		const FRotator YawOnly(0.0f, GetControlRotation().Yaw, 0.0f);
		AddMovementInput(YawOnly.GetForwardVector(), Value);
	});
	InputComponent->BindAxis("MoveRight", [this](float Value)
	{
		if (Value == 0.0f) return;
		const FRotator YawOnly(0.0f, GetControlRotation().Yaw, 0.0f);
		AddMovementInput(YawOnly.GetRightVector(), Value);
	});

	// Space = Jump (VK_SPACE = 0x20). Walking 중에만 effective (CharacterMovement::Jump 가 guard).
	InputComponent->AddActionMapping("Jump", 0x20);
	InputComponent->BindAction("Jump", EInputEvent::Pressed, [this]()
	{
		Jump();
	});
}

void ACharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CharacterMovement)
	{
		const InputSystem& Input = InputSystem::Get();
		CharacterMovement->SetCrouching(IsCrouchKeyDown(Input));
		CharacterMovement->SetSprinting(Input.GetKey(VK_SHIFT));
	}

	if (bAutoInputMouseLook)
	{
		const FInputSystemSnapshot InputSnapshot = UGameViewportClient::MakeCurrentGameInputSnapshot();
		const int DX = InputSnapshot.MouseDeltaX;
		const int DY = InputSnapshot.MouseDeltaY;
		if (DX != 0 || DY != 0)
		{
			// APawn::ControlRotation 누적. SpringArm 이 bUsePawnControlRotation 통해 이걸 사용.
			// capsule 회전은 옵션 (bUseControllerRotationYaw 등) — 아래 ApplyControllerRotationToRoot 가 처리.
			FRotator Rot = GetControlRotation();
			const float EffectiveMouseSensitivity = FLuaScriptManager::GetRuntimeMouseSensitivity();
			Rot.Yaw   += static_cast<float>(DX) * EffectiveMouseSensitivity;
			Rot.Pitch += static_cast<float>(DY) * EffectiveMouseSensitivity;
			Rot.Pitch  = std::clamp(Rot.Pitch, MinCameraPitch, MaxCameraPitch);
			SetControlRotation(Rot);
		}
	}

	// 같은 frame 안 ControlRotation 변경을 capsule (RootComponent) 에 즉시 반영 — 1 frame 지연 없음.
	// 옵션 충돌 가드:
	//   1) bOrientRotationToMovement = true → yaw 는 Movement::PhysOrientToMovement 가 처리.
	//   2) 직전 frame 에 root motion 이 yaw 를 적용했다 → 이번 frame 도 root motion 이 yaw 를
	//      이어받을 가능성이 큼. Character 가 control yaw 로 덮으면 root motion 회전이 즉시
	//      뒤집혀 토글링 됨 (turn-in-place / strafe anim 의 시각 손상). Movement 측에 양보.
	// 두 경우 모두 pitch/roll 만 apply, yaw 는 movement 에 양보.
	if (CapsuleComponent)
	{
		const bool bMovementHandlesYaw = CharacterMovement &&
			(CharacterMovement->bOrientRotationToMovement ||
			 CharacterMovement->HasYawDrivenByRootMotion());

		FRotator R = CapsuleComponent->GetRelativeRotation();
		bool bChanged = false;
		if (bUseControllerRotationYaw && !bMovementHandlesYaw)
		{
			R.Yaw   = ControlRotation.Yaw;
			bChanged = true;
		}
		if (bUseControllerRotationPitch)
		{
			R.Pitch = ControlRotation.Pitch;
			bChanged = true;
		}
		if (bUseControllerRotationRoll)
		{
			R.Roll  = ControlRotation.Roll;
			bChanged = true;
		}
		if (bChanged) CapsuleComponent->SetRelativeRotation(R);
	}

	// ── View tilt (TitanFall-style wall-run roll) ─────────────────────────────
	// CMC 가 "원하는 Roll deg" 만 내놓고, 여기서 critically damped 1st-order lerp 로 따라간다.
	// Camera 는 SpringArm 의 자식이라 local Roll 만 더해도 Yaw/Pitch (look 입력) 와 안 섞임.
	// ControlRotation.Roll 은 손대지 말 것 — Pawn look 수학과 hit-scan 방향이 깨짐.
	if (CharacterMovement && CharacterMovement->IsCameraTiltEnabled())
	{
		if (!CachedCameraForTilt)
		{
			CachedCameraForTilt = GetComponentByClass<UCameraComponent>();
		}
		if (CachedCameraForTilt)
		{
			const float TargetDeg = CharacterMovement->GetDesiredCameraRollDeg();
			const bool  bIncreasing = std::fabs(TargetDeg) > std::fabs(CurrentCameraTiltDeg);
			const float Hz = CharacterMovement->GetCameraTiltResponseHz(bIncreasing);
			// alpha = 1 - exp(-dt * Hz) — frame-rate independent critically damped lerp.
			const float Alpha = (Hz > 0.0f) ? (1.0f - std::exp(-DeltaTime * Hz)) : 1.0f;
			CurrentCameraTiltDeg += (TargetDeg - CurrentCameraTiltDeg) * Alpha;

			FRotator CamRot = CachedCameraForTilt->GetRelativeRotation();
			CamRot.Roll = CurrentCameraTiltDeg;
			CachedCameraForTilt->SetRelativeRotation(CamRot);
		}
	}
	else
	{
		// Disable 시 부드럽게 0 으로 복귀 후 손 뗌.
		if (CachedCameraForTilt && std::fabs(CurrentCameraTiltDeg) > 0.01f)
		{
			const float Alpha = 1.0f - std::exp(-DeltaTime * 9.0f);
			CurrentCameraTiltDeg += (0.0f - CurrentCameraTiltDeg) * Alpha;
			FRotator CamRot = CachedCameraForTilt->GetRelativeRotation();
			CamRot.Roll = CurrentCameraTiltDeg;
			CachedCameraForTilt->SetRelativeRotation(CamRot);
		}
	}
}
