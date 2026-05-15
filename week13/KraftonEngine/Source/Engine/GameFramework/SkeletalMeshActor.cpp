#include "SkeletalMeshActor.h"
#include "Runtime/Engine.h"
#include "Component/SkeletalMeshComponent.h"
#include "Mesh/MeshManager.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationMode.h"

IMPLEMENT_CLASS(ASkeletalMeshActor, AActor)

void ASkeletalMeshActor::BeginPlay()
{
	Super::BeginPlay();

	//SkeletalMeshComponent = GetComponentByClass<USkeletalMeshComponent>();

	// TODO(Phase 3 시각 검증): Phase 4 에 에디터 property 가 노출되면 제거.
	// Mode 가 명시적으로 설정되지 않은 액터 (= 시퀀스/FSM 미지정) 에 대해 mock wave 자동 적용.
	//if (SkeletalMeshComponent && SkeletalMeshComponent->GetAnimationMode() == EAnimationMode::None)
	//{
	//	USkeletalMesh* Mesh = SkeletalMeshComponent->GetSkeletalMesh();
	//	FSkeletalMesh*  Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	//	if (Asset && !Asset->Bones.empty())
	//	{
	//		UAnimSequence* Mock = UAnimSequence::CreateMockWaveSequence(Mesh, 2.0f, 15.0f);
	//		SkeletalMeshComponent->SetAnimation(Mock);
	//		SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	//	}
	//}
}

void ASkeletalMeshActor::InitDefaultComponents(const FString& SkeletalMeshFileName)
{
	SkeletalMeshComponent = AddComponent<USkeletalMeshComponent>();
	SetRootComponent(SkeletalMeshComponent);

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	USkeletalMesh* Asset = FMeshManager::LoadSkeletalMesh(SkeletalMeshFileName, Device);

	SkeletalMeshComponent->SetSkeletalMesh(Asset);
}
