#include "GameFramework/Character.h"

#include "Component/CapsuleComponent.h"
#include "Component/InputComponent.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Input/InputSystem.h"
#include "Mesh/MeshManager.h"
#include "Runtime/Engine.h"

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

	// Mouse look — delta X 만 yaw 에 반영. Pitch (위/아래) 는 minimal 단계에서 생략.
	if (bAutoInputMouseLook && CapsuleComponent)
	{
		const int DX = InputSystem::Get().MouseDeltaX();
		if (DX != 0)
		{
			const float DeltaYaw = static_cast<float>(DX) * MouseSensitivity;
			CapsuleComponent->Rotate(DeltaYaw, 0.0f);
		}
	}
}
