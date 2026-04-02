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
	std::filesystem::path SkyPath = FPaths::MeshDir() / "SkySphere.Model";
	UStaticMesh* SkyMesh = FObjManager::LoadModelStaticMeshAsset(FPaths::FromPath(SkyPath));

	SkySphereComponent = FObjectFactory::ConstructObject<USkyComponent>(this, "SkySphereComponent");
	SkySphereComponent->SetStaticMesh(SkyMesh);

	AddOwnedComponent(SkySphereComponent);
	SetRootComponent(SkySphereComponent);

	AActor::PostSpawnInitialize();
}
