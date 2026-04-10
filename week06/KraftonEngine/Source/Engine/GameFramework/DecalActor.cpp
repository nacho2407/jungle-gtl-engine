#include "DecalActor.h"
#include "Component/DecalComponent.h"

IMPLEMENT_CLASS(ADecalActor, AActor)

void ADecalActor::InitDefaultComponents()
{
	DecalComponent = AddComponent<UDecalComponent>();
	SetRootComponent(DecalComponent);
	//DecalComponent->SetTexture(FName());
}