#include "GameFramework/Actor/PhysicalAnimationActor.h"

#include "Animation/AnimationMode.h"
#include "Animation/Instance/CharacterAnimInstance.h"
#include "Component/Physics/PhysicalAnimationComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Mesh/MeshManager.h"
#include "Runtime/Engine.h"

void APhysicalAnimationActor::BeginPlay()
{
	Super::BeginPlay();

	ResolveComponents();
	BindPhysicalAnimationTarget();

	if (bAutoActivatePhysicalAnimation)
	{
		ActivatePhysicalAnimation();
	}
}

void APhysicalAnimationActor::InitDefaultComponents(const FString& SkeletalMeshFileName)
{
	SkeletalMeshComponent = AddComponent<USkeletalMeshComponent>();
	SetRootComponent(SkeletalMeshComponent);

	PhysicalAnimationComponent = AddComponent<UPhysicalAnimationComponent>();

	if (GEngine && SkeletalMeshComponent)
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		USkeletalMesh* Asset = FMeshManager::LoadSkeletalMesh(SkeletalMeshFileName, Device);
		SkeletalMeshComponent->SetSkeletalMesh(Asset);
		SkeletalMeshComponent->SetAnimInstanceClass(UCharacterAnimInstance::StaticClass());
		SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationCustom);
	}

	BindPhysicalAnimationTarget();

	if (bAutoActivatePhysicalAnimation && HasActorBegunPlay())
	{
		ActivatePhysicalAnimation();
	}
}

void APhysicalAnimationActor::ActivatePhysicalAnimation()
{
	ResolveComponents();
	BindPhysicalAnimationTarget();

	if (PhysicalAnimationComponent)
	{
		PhysicalAnimationComponent->ActivatePhysicalAnimation();
	}
}

void APhysicalAnimationActor::DeactivatePhysicalAnimation(bool bUseRecovery)
{
	if (PhysicalAnimationComponent)
	{
		PhysicalAnimationComponent->DeactivatePhysicalAnimation(bUseRecovery);
	}
}

void APhysicalAnimationActor::OnOwnedComponentRemoved(UActorComponent* Component)
{
	Super::OnOwnedComponentRemoved(Component);

	if (Component == SkeletalMeshComponent)
	{
		SkeletalMeshComponent = nullptr;
	}

	if (Component == PhysicalAnimationComponent)
	{
		PhysicalAnimationComponent = nullptr;
	}
}

void APhysicalAnimationActor::PostDuplicate()
{
	Super::PostDuplicate();
	ResolveComponents();
	BindPhysicalAnimationTarget();
}

void APhysicalAnimationActor::ResolveComponents()
{
	SkeletalMeshComponent = Cast<USkeletalMeshComponent>(GetRootComponent());
	if (!SkeletalMeshComponent)
	{
		SkeletalMeshComponent = GetComponentByClass<USkeletalMeshComponent>();
	}

	PhysicalAnimationComponent = GetComponentByClass<UPhysicalAnimationComponent>();
}

void APhysicalAnimationActor::BindPhysicalAnimationTarget()
{
	if (PhysicalAnimationComponent)
	{
		PhysicalAnimationComponent->SetSkeletalMeshComponent(SkeletalMeshComponent);
	}
}
