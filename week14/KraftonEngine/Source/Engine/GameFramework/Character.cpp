#include "GameFramework/Character.h"

#include "Component/CapsuleComponent.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/SkeletalMeshComponent.h"
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
