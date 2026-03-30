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
	CubeMesh = FObjManager::LoadObjStaticMeshAsset((FPaths::MeshDir() / "PrimitiveBox.obj").string().c_str());

	CubeMeshComponent = FObjectFactory::ConstructObject<UStaticMeshComponent>(this);
	CubeMeshComponent->SetStaticMesh(CubeMesh);

	AddOwnedComponent(CubeMeshComponent);

	AActor::PostSpawnInitialize();
}