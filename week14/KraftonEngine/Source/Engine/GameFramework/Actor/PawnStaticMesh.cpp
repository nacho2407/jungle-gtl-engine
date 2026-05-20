#include "GameFramework/Actor/PawnStaticMesh.h"
#include "Component/Primitive/StaticMeshComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Mesh/MeshManager.h"

void APawnStaticMesh::InitDefaultComponents(const FString& StaticMeshFileName)
{
	StaticMeshComponent = AddComponent<UStaticMeshComponent>();
	SetRootComponent(StaticMeshComponent);

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	UStaticMesh* Asset = FMeshManager::LoadStaticMesh(StaticMeshFileName, Device);
	StaticMeshComponent->SetStaticMesh(Asset);
}

void APawnStaticMesh::PostDuplicate()
{
	StaticMeshComponent = Cast<UStaticMeshComponent>(GetRootComponent());
}
