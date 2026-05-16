#include "SkeletalMeshActor.h"
#include "Runtime/Engine.h"
#include "Component/SkeletalMeshComponent.h"
#include "Mesh/MeshManager.h"
#include "Animation/AnimationMode.h"
#include "Animation/CharacterAnimInstance.h"

IMPLEMENT_CLASS(ASkeletalMeshActor, AActor)

void ASkeletalMeshActor::BeginPlay()
{
	Super::BeginPlay();

	//SkeletalMeshComponent = GetComponentByClass<USkeletalMeshComponent>();
}

void ASkeletalMeshActor::InitDefaultComponents(const FString& SkeletalMeshFileName)
{
	SkeletalMeshComponent = AddComponent<USkeletalMeshComponent>();
	SetRootComponent(SkeletalMeshComponent);

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	USkeletalMesh* Asset = FMeshManager::LoadSkeletalMesh(SkeletalMeshFileName, Device);

	SkeletalMeshComponent->SetSkeletalMesh(Asset);

	// Phase 5 데모: 확장 FSM (UCharacterAnimInstance) 자동 wiring.
	// 순서 — Class 먼저 (Mode==None 이라 재초기화 미발생) → Mode=Custom 전환 시 InitializeAnimation 1회.
	SkeletalMeshComponent->SetAnimInstanceClass(UCharacterAnimInstance::StaticClass());
	SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationCustom);
}
