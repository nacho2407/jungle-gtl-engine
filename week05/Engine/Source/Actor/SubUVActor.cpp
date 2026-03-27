#include "SubUVActor.h"
#include "Component/SubUVComponent.h"
#include "Object/ObjectFactory.h"
#include "Object/Class.h"

IMPLEMENT_RTTI(ASubUVActor, AActor)

void ASubUVActor::PostSpawnInitialize()
{
	SubUVComponent = FObjectFactory::ConstructObject<USubUVComponent>(this, "SubUVComponent");
	AddOwnedComponent(SubUVComponent);

	SetActorLocation(FVector(0.0f, 0.0f, 0.0f));

	if (SubUVComponent)
	{
		SubUVComponent->SetSize(FVector2(1.0f, 1.0f));
		SubUVComponent->SetFirstFrame(0);
		SubUVComponent->SetLastFrame(11);
	}

	AActor::PostSpawnInitialize();
}