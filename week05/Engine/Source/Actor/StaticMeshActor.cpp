#include "StaticMeshActor.h"

#include "Actor.h"
#include "Object/Class.h"
#include "Component/StaticMeshComponent.h"
#include "Component/RandomColorComponent.h"

IMPLEMENT_RTTI(AStaticMeshActor, AActor)
void AStaticMeshActor::PostSpawnInitialize()
{
	StaticMeshComp = FObjectFactory::ConstructObject<UStaticMeshComponent>(this);
	AddOwnedComponent(StaticMeshComp);

	AActor::PostSpawnInitialize();
}
