#include "PlaneActor.h"
#include "Component/PlaneComponent.h"
#include "Component/RandomColorComponent.h"
#include "Object/ObjectFactory.h"
#include "Object/Class.h"

IMPLEMENT_RTTI(APlaneActor, AActor)

void APlaneActor::PostSpawnInitialize()
{
	PrimitiveComponent = FObjectFactory::ConstructObject<UPlaneComponent>(this);
	AddOwnedComponent(PrimitiveComponent);

	if (bUseRandomColor)
	{
		RandomColorComponent = FObjectFactory::ConstructObject<URandomColorComponent>(this);
		AddOwnedComponent(RandomColorComponent);
	}

	AActor::PostSpawnInitialize();
}
