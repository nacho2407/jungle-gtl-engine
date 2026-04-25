#include "PointLightActor.h"
#include "Component/BillboardComponent.h"
#include "Component/Light/PointLightComponent.h"
#include "Materials/MaterialManager.h"

IMPLEMENT_CLASS(APointLightActor, AActor)

void APointLightActor::InitDefaultComponents()
{
	LightComponent = AddComponent<UPointLightComponent>();
	SetRootComponent(LightComponent);

	BillboardComponent = LightComponent->EnsureEditorBillboard();
}
