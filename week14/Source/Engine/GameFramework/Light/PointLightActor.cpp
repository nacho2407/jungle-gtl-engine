#include "PointLightActor.h"
#include "Component/Primitive/BillboardComponent.h"
#include "Component/Light/PointLightComponent.h"
#include "Materials/MaterialManager.h"

void APointLightActor::InitDefaultComponents()
{
	LightComponent = AddComponent<UPointLightComponent>();
	SetRootComponent(LightComponent);

	BillboardComponent = LightComponent->EnsureEditorBillboard();
}
