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

void APointLightActor::OnOwnedComponentRemoved(UActorComponent* Component)
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

void APointLightActor::PostDuplicate()
{
	Super::PostDuplicate();
	LightComponent = Cast<UPointLightComponent>(GetRootComponent());
	if (!LightComponent)
	{
		LightComponent = GetComponentByClass<UPointLightComponent>();
	}
	BillboardComponent = GetComponentByClass<UBillboardComponent>();
}
