#include "GameFramework/DecalActor.h"

#include "Component/BillboardComponent.h"
#include "Component/DecalComponent.h"
#include "Component/TextRenderComponent.h"

IMPLEMENT_CLASS(ADecalActor, AActor)

void ADecalActor::InitDefaultComponents()
{
	if (!DecalComponent)
	{
		DecalComponent = AddComponent<UDecalComponent>();
		SetRootComponent(DecalComponent);
	}

	if (!IconBillboardComponent)
	{
		IconBillboardComponent = AddComponent<UBillboardComponent>();
		IconBillboardComponent->AttachToComponent(DecalComponent);
		IconBillboardComponent->SetTexture(FName("DecalActor"));
		IconBillboardComponent->SetSpriteSize(1.0f, 1.0f);
		IconBillboardComponent->SetVisibility(true);
		IconBillboardComponent->SetSupportsOutline(false);
	}

	if (!TextRenderComponent)
	{
		TextRenderComponent = AddComponent<UTextRenderComponent>();
		TextRenderComponent->AttachToComponent(DecalComponent);
		TextRenderComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 1.1f));
		TextRenderComponent->SetText("UUID : " + TextRenderComponent->GetOwnerUUIDToString());
		TextRenderComponent->SetFont(FName("Default"));
		TextRenderComponent->SetVisibility(true);
		TextRenderComponent->SetSupportsOutline(false);
	}
}
