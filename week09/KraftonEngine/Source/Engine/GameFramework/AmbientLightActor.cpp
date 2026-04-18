#include "AmbientLightActor.h"
#include "Component/AmbientLightComponent.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(AAmbientLightActor, AActor)

AAmbientLightActor::AAmbientLightActor()
{
	bNeedsTick = false;
	bTickInEditor = false;
}

void AAmbientLightActor::InitDefaultComponents()
{
    AmbientLightComponent = AddComponent<UAmbientLightComponent>();
    SetRootComponent(AmbientLightComponent);
}