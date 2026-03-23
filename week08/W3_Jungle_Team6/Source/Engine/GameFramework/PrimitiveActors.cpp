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
	auto* Cube = AddComponent<UCubeComponent>();
	SetRootComponent(Cube);

	UTextRenderComponent* Text = AddComponent<UTextRenderComponent>();
	Text->AttachToComponent(Cube);
	Text->SetText(GetFName().ToString());
	Text->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
}

void ASphereActor::InitDefaultComponents()
{
	auto* Sphere = AddComponent<USphereComponent>();
	SetRootComponent(Sphere);

	UTextRenderComponent* Text = AddComponent<UTextRenderComponent>();
	Text->AttachToComponent(Sphere);
	Text->SetText(GetFName().ToString());
	Text->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
}

void APlaneActor::InitDefaultComponents()
{
	auto* Plane = AddComponent<UPlaneComponent>();
	SetRootComponent(Plane);

	UTextRenderComponent* Text = AddComponent<UTextRenderComponent>();
	Text->AttachToComponent(Plane);
	Text->SetText(GetFName().ToString());
	Text->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
}
