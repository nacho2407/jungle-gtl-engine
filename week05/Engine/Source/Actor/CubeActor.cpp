#include "CubeActor.h"

#include "Asset/ObjManager.h"
#include "Component/StaticMeshComponent.h"
#include "Core/Paths.h"
#include "Object/ObjectFactory.h"
#include "Object/Class.h"

IMPLEMENT_RTTI(ACubeActor, AActor)

void ACubeActor::PostSpawnInitialize()
{
	UStaticMesh* CubeMesh = nullptr;
	CubeMesh = FObjManager::LoadModelStaticMeshAsset(FPaths::FromPath(FPaths::MeshDir() / "PrimitiveBox.Model"));

	CubeMeshComponent = FObjectFactory::ConstructObject<UStaticMeshComponent>(this, "StaticMeshComponent");
	CubeMeshComponent->SetStaticMesh(CubeMesh);

	AddOwnedComponent(CubeMeshComponent);

	AActor::PostSpawnInitialize();
}
