#include "DirectionalLightActor.h"
#include "Component/Primitive/BillboardComponent.h"
#include "Component/Light/DirectionalLightComponent.h"
#include "Materials/MaterialManager.h"
void ADirectionalLightActor::InitDefaultComponents()
{
	LightComponent = AddComponent<UDirectionalLightComponent>();
	SetRootComponent(LightComponent);

	BillboardComponent = LightComponent->EnsureEditorBillboard();
}
