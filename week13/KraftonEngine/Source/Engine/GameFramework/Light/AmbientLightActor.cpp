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

void AAmbientLightActor::OnOwnedComponentRemoved(UActorComponent* Component)
{
	Super::OnOwnedComponentRemoved(Component);
	if (Component == LightComponent)
	{
		LightComponent = nullptr;
	}
	if (Component == BillboardComponent)
	{
		BillboardComponent = nullptr;
	}
}

void AAmbientLightActor::PostDuplicate()
{
	Super::PostDuplicate();
	LightComponent = Cast<UAmbientLightComponent>(GetRootComponent());
	if (!LightComponent)
	{
		LightComponent = GetComponentByClass<UAmbientLightComponent>();
	}
	BillboardComponent = GetComponentByClass<UBillboardComponent>();
}
