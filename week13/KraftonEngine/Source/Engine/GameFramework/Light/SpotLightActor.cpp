#include "SpotLightActor.h"
#include "Component/BillboardComponent.h"
#include "Component/Light/SpotLightComponent.h"
#include "Materials/MaterialManager.h"

void ASpotLightActor::InitDefaultComponents()
{
	LightComponent = AddComponent<USpotLightComponent>();
	SetRootComponent(LightComponent);

	BillboardComponent = LightComponent->EnsureEditorBillboard();
}
