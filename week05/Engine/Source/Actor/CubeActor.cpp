#include "CubeActor.h"
#include "Component/CubeComponent.h"
#include "Component/RandomColorComponent.h"
#include "Object/ObjectFactory.h"
#include "Object/Class.h"

IMPLEMENT_RTTI(ACubeActor, AActor)

void ACubeActor::PostSpawnInitialize()
{
	PrimitiveComponent = FObjectFactory::ConstructObject<UCubeComponent>(this);
	AddOwnedComponent(PrimitiveComponent);

	if (bUseRandomColor)
	{
		RandomColorComponent = FObjectFactory::ConstructObject<URandomColorComponent>(this);
		AddOwnedComponent(RandomColorComponent);
	}

	AActor::PostSpawnInitialize();
}
