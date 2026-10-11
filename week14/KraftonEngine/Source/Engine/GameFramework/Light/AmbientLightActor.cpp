#include "AmbientLightActor.h"
#include "Component/Primitive/BillboardComponent.h"
#include "Component/Light/LightComponentBase.h"
#include "Component/Light/AmbientLightComponent.h"
#include "Materials/MaterialManager.h"

void AAmbientLightActor::InitDefaultComponents()
{
	LightComponent = AddComponent<UAmbientLightComponent>();
	SetRootComponent(LightComponent);

	BillboardComponent = LightComponent->EnsureEditorBillboard();
}
