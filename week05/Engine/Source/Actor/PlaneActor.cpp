#include "PlaneActor.h"

#include "Asset/ObjManager.h"
#include "Component/StaticMeshComponent.h"
#include "Core/Paths.h"
#include "Object/ObjectFactory.h"
#include "Object/Class.h"

IMPLEMENT_RTTI(APlaneActor, AActor)

void APlaneActor::PostSpawnInitialize()
{
	UStaticMesh* PlaneMesh = nullptr;
	PlaneMesh = FObjManager::LoadObjStaticMeshAsset(FPaths::FromPath(FPaths::MeshDir() / "PrimitivePlane.obj"));

	PlaneMeshComponent = FObjectFactory::ConstructObject<UStaticMeshComponent>(this);
	PlaneMeshComponent->SetStaticMesh(PlaneMesh);

	AddOwnedComponent(PlaneMeshComponent);

	AActor::PostSpawnInitialize();
}
