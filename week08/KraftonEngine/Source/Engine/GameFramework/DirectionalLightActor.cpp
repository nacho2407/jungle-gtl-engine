#include "DirectionalLightActor.h"
#include "Component/DirectionalLightComponent.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(ADirectionalLightActor, AActor)

ADirectionalLightActor::ADirectionalLightActor()
{
	bNeedsTick = false;
	bTickInEditor = false;
}

void ADirectionalLightActor::InitDefaultComponents()
{
    DirectionalLightComponent = AddComponent<UDirectionalLightComponent>();
    SetRootComponent(DirectionalLightComponent);
}