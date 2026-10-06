#include "HeightFogActor.h"
#include "Component/Primitive/HeightFogComponent.h"
#include "Component/Primitive/BillboardComponent.h"
#include "Materials/MaterialManager.h"

AHeightFogActor::AHeightFogActor()
{
}

void AHeightFogActor::InitDefaultComponents()
{
	FogComponent = AddComponent<UHeightFogComponent>();
	SetRootComponent(FogComponent);

	BillboardComponent = FogComponent->EnsureEditorBillboard();
}

void AHeightFogActor::OnOwnedComponentRemoved(UActorComponent* Component)
{
	Super::OnOwnedComponentRemoved(Component);
	if (Component == FogComponent)
	{
		FogComponent = nullptr;
	}
	if (Component == BillboardComponent)
	{
		BillboardComponent = nullptr;
	}
}

void AHeightFogActor::PostDuplicate()
{
	Super::PostDuplicate();
	FogComponent = Cast<UHeightFogComponent>(GetRootComponent());
	if (!FogComponent)
	{
		FogComponent = GetComponentByClass<UHeightFogComponent>();
	}
	BillboardComponent = GetComponentByClass<UBillboardComponent>();
}
