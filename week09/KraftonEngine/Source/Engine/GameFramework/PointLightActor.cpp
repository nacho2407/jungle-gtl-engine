#include "PointLightActor.h"
#include "Component/PointLightComponent.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(APointLightActor, AActor)

APointLightActor::APointLightActor()
{
	bNeedsTick = false;
	bTickInEditor = false;
}

void APointLightActor::InitDefaultComponents()
{
    PointLightComponent = AddComponent<UPointLightComponent>();
    SetRootComponent(PointLightComponent);
}