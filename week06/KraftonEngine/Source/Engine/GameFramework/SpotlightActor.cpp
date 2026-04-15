#include "GameFramework/SpotlightActor.h"

#include "Component/BillboardComponent.h"
#include "Component/DecalComponent.h"
#include "Component/TextRenderComponent.h"

IMPLEMENT_CLASS(ASpotlightActor, AActor)

void ASpotlightActor::InitDefaultComponents() {

	if (!BillboardComponent)
	{
		BillboardComponent = AddComponent<UBillboardComponent>();
		BillboardComponent->SetTexture(FName("spotlight_Spot"));
		BillboardComponent->SetSpriteSize(0.5f, 0.5f);
		BillboardComponent->SetIgnoreOwnerScale(true);
		BillboardComponent->SetVisibility(true);
		BillboardComponent->SetSupportsOutline(false);
		BillboardComponent->SetHitTestEnabled(true);
		SetRootComponent(BillboardComponent);
	}

	if (!DecalComponent)
	{
		DecalComponent = AddComponent<UDecalComponent>();
		DecalComponent->AttachToComponent(BillboardComponent);
		DecalComponent->SetHitTestEnabled(false);
		DecalComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -1.5f));
		DecalComponent->SetRelativeRotation(FVector(0.0f, 90.0f, 0.0f));
		DecalComponent->SetRelativeScale(FVector(3.0f, 1.0f, 1.0f));
		DecalComponent->SetTexture(FName("spotlight_Light"));
	}

	if (!TextRenderComponent)
	{
		TextRenderComponent = AddComponent<UTextRenderComponent>();
		TextRenderComponent->AttachToComponent(BillboardComponent);
		TextRenderComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 1.1f));
		TextRenderComponent->SetText("UUID : " + TextRenderComponent->GetOwnerUUIDToString());
		TextRenderComponent->SetFont(FName("Default"));
		TextRenderComponent->SetVisibility(true);
		TextRenderComponent->SetSupportsOutline(false);
		TextRenderComponent->SetHitTestEnabled(false);
	}
}