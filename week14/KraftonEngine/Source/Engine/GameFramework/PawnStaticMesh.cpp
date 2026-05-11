#include "GameFramework/PawnStaticMesh.h"
#include "Component/StaticMeshComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Mesh/MeshManager.h"

IMPLEMENT_CLASS(APawnStaticMesh, APawn)

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
