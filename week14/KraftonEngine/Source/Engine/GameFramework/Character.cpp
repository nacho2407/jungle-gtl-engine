#include "GameFramework/Character.h"

#include "Component/CapsuleComponent.h"
#include "Component/InputComponent.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/SpringArmComponent.h"
#include "Input/InputSystem.h"
#include "Math/Rotator.h"
#include "Mesh/MeshManager.h"
#include "Runtime/Engine.h"

#include <algorithm>

IMPLEMENT_CLASS(ACharacter, APawn)

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

	InputComponent->BindAxis("MoveForward", [this](float Value)
	{
		if (Value == 0.0f || !CapsuleComponent) return;
		// XY 평면만 — Z 성분 제거. (capsule 회전이 yaw 만이라 사실상 Z=0 이지만 안전.)
		FVector Fwd = CapsuleComponent->GetForwardVector();
		Fwd.Z = 0.0f;
		AddMovementInput(Fwd, Value);
	});
	InputComponent->BindAxis("MoveRight", [this](float Value)
	{
		if (Value == 0.0f || !CapsuleComponent) return;
		FVector Right = CapsuleComponent->GetRightVector();
		Right.Z = 0.0f;
		AddMovementInput(Right, Value);
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

	if (!bAutoInputMouseLook) return;

	const InputSystem& In = InputSystem::Get();

	// Yaw — capsule 자체 회전. Mesh / SpringArm / Camera 가 자동 follow.
	if (CapsuleComponent)
	{
		const int DX = In.MouseDeltaX();
		if (DX != 0)
		{
			const float DeltaYaw = static_cast<float>(DX) * MouseSensitivity;
			CapsuleComponent->Rotate(DeltaYaw, 0.0f);
		}
	}

	// Pitch — SpringArm 의 relative pitch 만. capsule 은 pitch 영향 없음 (캐릭터 안 누움).
	// SpringArm 없는 ACharacter 자식 (예: 카메라 없는 베이스) 은 lazy 조회 결과 nullptr — no-op.
	if (!CameraSpringArm)
	{
		CameraSpringArm = GetComponentByClass<USpringArmComponent>();
	}
	if (CameraSpringArm)
	{
		const int DY = In.MouseDeltaY();
		if (DY != 0)
		{
			// 마우스 아래 → 카메라 위 (UE 기본 — invert 토글 추후).
			CameraPitch += static_cast<float>(DY) * MouseSensitivity;
			CameraPitch = std::clamp(CameraPitch, MinCameraPitch, MaxCameraPitch);
			CameraSpringArm->SetRelativeRotation(FRotator(CameraPitch, 0.0f, 0.0f));
		}
	}
}
