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

void ADirectionalLightActor::OnOwnedComponentRemoved(UActorComponent* Component)
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

void ADirectionalLightActor::PostDuplicate()
{
	Super::PostDuplicate();
	LightComponent = Cast<UDirectionalLightComponent>(GetRootComponent());
	if (!LightComponent)
	{
		LightComponent = GetComponentByClass<UDirectionalLightComponent>();
	}
	BillboardComponent = GetComponentByClass<UBillboardComponent>();
}
