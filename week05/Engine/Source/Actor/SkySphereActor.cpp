#include "SkySphereActor.h"

#include "PlaneActor.h"

#include "Asset/ObjManager.h"
#include "Component/SkyComponent.h"
#include "Core/Paths.h"
#include "Object/ObjectFactory.h"
#include "Object/Class.h"

IMPLEMENT_RTTI(ASkySphereActor, AActor)

void ASkySphereActor::PostSpawnInitialize()
{
	SkySphereComponent = FObjectFactory::ConstructObject<USkyComponent>(this);

	AddOwnedComponent(SkySphereComponent);

	AActor::PostSpawnInitialize();
}