#include "SkeletalMeshActor.h"
#include "Runtime/Engine.h"
#include "Component/SkeletalMeshComponent.h"
#include "Mesh/FbxManager.h"

IMPLEMENT_CLASS(ASkeletalMeshActor, AActor)

void ASkeletalMeshActor::BeginPlay()
{
	Super::BeginPlay();
}

void ASkeletalMeshActor::InitDefaultComponents(const FString& SkeletalMeshFileName)
{
	SkeletalMeshComponent = AddComponent<USkeletalMeshComponent>();
	SetRootComponent(SkeletalMeshComponent);

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	USkeletalMesh* Asset = FFbxManager::LoadFbxSkeletalMesh(SkeletalMeshFileName, Device);

	SkeletalMeshComponent->SetSkeletalMesh(Asset);
}
