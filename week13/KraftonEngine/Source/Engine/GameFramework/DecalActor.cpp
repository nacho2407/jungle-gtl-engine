#include "DecalActor.h"
#include "Component/DecalComponent.h"
#include "Component/TextRenderComponent.h"

IMPLEMENT_CLASS(ADecalActor, AActor)

ADecalActor::ADecalActor()
	: DecalComponent(nullptr)
{
	bNeedsTick = true;
	bTickInEditor = true;
}

void ADecalActor::InitDefaultComponents()
{
	DecalComponent = AddComponent<UDecalComponent>();
	SetRootComponent(DecalComponent);
	
	// UUID 텍스트 표시
	TextRenderComponent = AddComponent<UTextRenderComponent>();
	TextRenderComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 1.3f));
	TextRenderComponent->SetText("UUID : " + TextRenderComponent->GetOwnerUUIDToString());
	TextRenderComponent->AttachToComponent(DecalComponent);
	TextRenderComponent->SetFont(FName("Default"));
}
