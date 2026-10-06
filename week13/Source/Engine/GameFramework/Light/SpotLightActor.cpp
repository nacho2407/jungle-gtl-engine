#include "SpotLightActor.h"
#include "Component/Primitive/BillboardComponent.h"
#include "Component/Light/SpotLightComponent.h"
#include "Materials/MaterialManager.h"

void ASpotLightActor::InitDefaultComponents()
{
	LightComponent = AddComponent<USpotLightComponent>();
	SetRootComponent(LightComponent);

	BillboardComponent = LightComponent->EnsureEditorBillboard();
}

void ASpotLightActor::OnOwnedComponentRemoved(UActorComponent* Component)
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

void ASpotLightActor::PostDuplicate()
{
	Super::PostDuplicate();
	LightComponent = Cast<USpotLightComponent>(GetRootComponent());
	if (!LightComponent)
	{
		LightComponent = GetComponentByClass<USpotLightComponent>();
	}
	BillboardComponent = GetComponentByClass<UBillboardComponent>();
}
