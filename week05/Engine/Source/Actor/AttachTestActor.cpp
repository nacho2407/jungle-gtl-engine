#include "AttachTestActor.h"
#include "Component/SphereComponent.h"
#include "Component/CubeComponent.h"
#include "Object/ObjectFactory.h"
#include "Object/Class.h"

IMPLEMENT_RTTI(AAttachTestActor, AActor)

void AAttachTestActor::PostSpawnInitialize()
{
	SphereComponent = FObjectFactory::ConstructObject<USphereComponent>(this);
	CubeComponent = FObjectFactory::ConstructObject<UCubeComponent>(this);

	CubeComponent->AttachTo(SphereComponent);
	CubeComponent->SetRelativeTransform({ FRotator::MakeFromEuler({ 45.0f, 45.0f, 45.0f }), {0.0f, 0.0f, 2.0f}, {0.5f, 0.5f, 0.5f} });

	AddOwnedComponent(SphereComponent);
	AddOwnedComponent(CubeComponent);

	AActor::PostSpawnInitialize();
}
