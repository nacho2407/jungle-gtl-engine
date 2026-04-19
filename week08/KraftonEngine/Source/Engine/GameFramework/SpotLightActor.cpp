#include "SpotLightActor.h"
#include "Component/SpotLightComponent.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(ASpotLightActor, AActor)

ASpotLightActor::ASpotLightActor()
{
	bNeedsTick = false;
	bTickInEditor = false;
}

void ASpotLightActor::InitDefaultComponents()
{
    SpotLightComponent = AddComponent<USpotLightComponent>();
    SetRootComponent(SpotLightComponent);
}