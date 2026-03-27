#include "SphereActor.h"
#include "Component/SphereComponent.h"
#include "Component/RandomColorComponent.h"
#include "Object/ObjectFactory.h"
#include "Object/Class.h"

IMPLEMENT_RTTI(ASphereActor, AActor)

void ASphereActor::PostSpawnInitialize()
{
	PrimitiveComponent = FObjectFactory::ConstructObject<USphereComponent>(this);
	AddOwnedComponent(PrimitiveComponent);

	if (bUseRandomColor)
	{
		RandomColorComponent = FObjectFactory::ConstructObject<URandomColorComponent>(this);
		AddOwnedComponent(RandomColorComponent);
	}

	AActor::PostSpawnInitialize();
}
