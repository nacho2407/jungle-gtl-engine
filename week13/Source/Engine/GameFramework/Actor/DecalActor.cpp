#include "DecalActor.h"
#include "Component/Primitive/DecalComponent.h"
#include "Component/Primitive/BillboardComponent.h"
#include "Component/Primitive/TextRenderComponent.h"
#include "Materials/MaterialManager.h"

ADecalActor::ADecalActor()
	: DecalComponent(nullptr)
{
	bNeedsTick = true;
	bTickInEditor = true;
}

void ADecalActor::InitDefaultComponents()
{
	DecalComponent = AddComponent<UDecalComponent>();
	auto Material = FMaterialManager::Get().GetOrCreateMaterial(DefaultDecalMaterialPath);
	DecalComponent->SetMaterial(Material);
	SetRootComponent(DecalComponent);

	BillboardComponent = DecalComponent->EnsureEditorBillboard();
	
	// UUID 텍스트 표시
	TextRenderComponent = AddComponent<UTextRenderComponent>();
	TextRenderComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 1.3f));
	TextRenderComponent->SetText("UUID : " + TextRenderComponent->GetOwnerUUIDToString());
	TextRenderComponent->AttachToComponent(DecalComponent);
	TextRenderComponent->SetFont(FName("Default"));
}

void ADecalActor::OnOwnedComponentRemoved(UActorComponent* Component)
{
	Super::OnOwnedComponentRemoved(Component);
	if (Component == DecalComponent)
	{
		DecalComponent = nullptr;
	}
	if (Component == BillboardComponent)
	{
		BillboardComponent = nullptr;
	}
	if (Component == TextRenderComponent)
	{
		TextRenderComponent = nullptr;
	}
}

void ADecalActor::PostDuplicate()
{
	Super::PostDuplicate();
	DecalComponent = Cast<UDecalComponent>(GetRootComponent());
	if (!DecalComponent)
	{
		DecalComponent = GetComponentByClass<UDecalComponent>();
	}
	BillboardComponent = GetComponentByClass<UBillboardComponent>();
	TextRenderComponent = GetComponentByClass<UTextRenderComponent>();
}
