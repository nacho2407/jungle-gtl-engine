#include "GameFramework/Character.h"

#include "Component/CapsuleComponent.h"
#include "Component/InputComponent.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Input/InputSystem.h"
#include "Math/Rotator.h"
#include "Mesh/MeshManager.h"
#include "Runtime/Engine.h"

#include <algorithm>
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
}

void ACharacter::AddMovementInput(const FVector& WorldDirection, float ScaleValue)
{
	if (CharacterMovement)
	{
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

	if (bAutoInputMouseLook)
	{
		const InputSystem& In = InputSystem::Get();
		const int DX = In.MouseDeltaX();
		const int DY = In.MouseDeltaY();
		if (DX != 0 || DY != 0)
		{
			// APawn::ControlRotation 누적. SpringArm 이 bUsePawnControlRotation 통해 이걸 사용.
			// capsule 회전은 옵션 (bUseControllerRotationYaw 등) — 아래 ApplyControllerRotationToRoot 가 처리.
			FRotator Rot = GetControlRotation();
			Rot.Yaw   += static_cast<float>(DX) * MouseSensitivity;
			Rot.Pitch += static_cast<float>(DY) * MouseSensitivity;
			Rot.Pitch  = std::clamp(Rot.Pitch, MinCameraPitch, MaxCameraPitch);
			SetControlRotation(Rot);
		}
	}

	// 같은 frame 안 ControlRotation 변경을 capsule (RootComponent) 에 즉시 반영 — 1 frame 지연 없음.
	// 옵션 충돌 가드: CharacterMovement::bOrientRotationToMovement = true 면 yaw 는 Movement
	// 의 PhysOrientToMovement 가 처리. ApplyControllerRotationToRoot 가 같은 frame 에 yaw 를
	// ControlYaw 로 덮어쓰면 두 곳에서 토글 → 캐릭터가 이동 방향 안 보고 끊김 현상.
	// → pitch/roll 만 apply, yaw 는 movement 에 양보.
	if (CapsuleComponent)
	{
		const bool bMovementHandlesYaw = CharacterMovement && CharacterMovement->bOrientRotationToMovement;

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
}
