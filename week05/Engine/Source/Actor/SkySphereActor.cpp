#include "SkySphereActor.h"

#include "PlaneActor.h"

#include "Asset/ObjManager.h"
#include "Component/SkyComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Core/Paths.h"
#include "Object/ObjectFactory.h"
 
#include "Object/Class.h"

IMPLEMENT_RTTI(ASkySphereActor, AActor)

void ASkySphereActor::PostSpawnInitialize()
{
	std::filesystem::path SkyPath = FPaths::MeshDir() / "SkySphere.obj";
	UStaticMesh* SkyMesh = FObjManager::LoadObjStaticMeshAsset(SkyPath.string().c_str());

	SkySphereComponent = FObjectFactory::ConstructObject<USkyComponent>(this);
	SkySphereComponent->SetStaticMesh(SkyMesh);

	AddOwnedComponent(SkySphereComponent);
	SetRootComponent(SkySphereComponent);

	AActor::PostSpawnInitialize();
}