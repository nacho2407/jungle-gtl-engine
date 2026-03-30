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
	PlaneMesh = FObjManager::LoadObjStaticMeshAsset((FPaths::MeshDir() / "PrimitivePlane.obj").string().c_str());

	PlaneMeshComponent = FObjectFactory::ConstructObject<UStaticMeshComponent>(this);
	PlaneMeshComponent->SetStaticMesh(PlaneMesh);

	AddOwnedComponent(PlaneMeshComponent);

	AActor::PostSpawnInitialize();
}