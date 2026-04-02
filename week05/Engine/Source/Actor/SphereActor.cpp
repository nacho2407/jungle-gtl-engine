#include "SphereActor.h"

#include "Asset/ObjManager.h"
#include "Component/StaticMeshComponent.h"
#include "Core/Paths.h"
#include "Object/ObjectFactory.h"
#include "Object/Class.h"

IMPLEMENT_RTTI(ASphereActor, AActor)

void ASphereActor::PostSpawnInitialize()
{
	UStaticMesh* SphereMesh = nullptr;
	SphereMesh = FObjManager::LoadModelStaticMeshAsset(FPaths::FromPath(FPaths::MeshDir() / "PrimitiveSphere.Model"));

	SphereMeshComponent = FObjectFactory::ConstructObject<UStaticMeshComponent>(this, "StaticMeshComponent");
	SphereMeshComponent->SetStaticMesh(SphereMesh);

	AddOwnedComponent(SphereMeshComponent);

	AActor::PostSpawnInitialize();
}
