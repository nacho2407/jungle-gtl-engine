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
	PlaneMesh = FObjManager::LoadModelStaticMeshAsset(FPaths::FromPath(FPaths::MeshDir() / "PrimitivePlane.Model"));

	PlaneMeshComponent = FObjectFactory::ConstructObject<UStaticMeshComponent>(this, "StaticMeshComponent");
	PlaneMeshComponent->SetStaticMesh(PlaneMesh);

	AddOwnedComponent(PlaneMeshComponent);

	AActor::PostSpawnInitialize();
}

void APlaneActor::FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const
{
	AActor::FixupDuplicatedReferences(DuplicatedObject, Context);
	static_cast<APlaneActor*>(DuplicatedObject)->PlaneMeshComponent = Context.FindDuplicate(PlaneMeshComponent);
}
