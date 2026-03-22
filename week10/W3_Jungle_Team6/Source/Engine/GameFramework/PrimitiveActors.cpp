#include "GameFramework/PrimitiveActors.h"

#include "Component/PrimitiveComponent.h"
#include "Component/TextRenderComponent.h"

DEFINE_CLASS(ACubeActor, AActor)
REGISTER_FACTORY(ACubeActor)

DEFINE_CLASS(ASphereActor, AActor)
REGISTER_FACTORY(ASphereActor)

DEFINE_CLASS(APlaneActor, AActor)
REGISTER_FACTORY(APlaneActor)

void ACubeActor::InitDefaultComponents()
{
	AddComponent<UCubeComponent>();
	UTextRenderComponent* Text = AddComponent<UTextRenderComponent>();
	Text->SetText(GetFName().ToString());
	Text->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
}

void ASphereActor::InitDefaultComponents()
{
	AddComponent<USphereComponent>();
	UTextRenderComponent* Text = AddComponent<UTextRenderComponent>();
	Text->SetText(GetFName().ToString());
	Text->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
}

void APlaneActor::InitDefaultComponents()
{
	AddComponent<UPlaneComponent>();
	UTextRenderComponent* Text = AddComponent<UTextRenderComponent>();
	Text->SetText(GetFName().ToString());
	Text->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
}
