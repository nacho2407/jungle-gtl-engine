#include "GameFramework/Character.h"

#include "Component/CapsuleComponent.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Input/InputSystem.h"
#include "Mesh/MeshManager.h"
#include "Runtime/Engine.h"

#include <windows.h>

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

void ACharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bAutoInputWASD || !CharacterMovement) return;

	// minimal 자동 입력 — world axes 기준. W=+X(앞), S=-X, A=-Y, D=+Y.
	// 카메라/yaw 기반 회전은 게임 시 자식이 override 해서 처리.
	const InputSystem& In = InputSystem::Get();
	if (In.GetKey('W')) AddMovementInput(FVector(1.0f, 0.0f, 0.0f),  1.0f);
	if (In.GetKey('S')) AddMovementInput(FVector(1.0f, 0.0f, 0.0f), -1.0f);
	if (In.GetKey('D')) AddMovementInput(FVector(0.0f, 1.0f, 0.0f),  1.0f);
	if (In.GetKey('A')) AddMovementInput(FVector(0.0f, 1.0f, 0.0f), -1.0f);
}
