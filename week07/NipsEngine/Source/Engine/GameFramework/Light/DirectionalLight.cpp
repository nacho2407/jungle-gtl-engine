#include "DirectionalLight.h"

#include "Component/BillboardComponent.h"
#include "Component/Light/DirectionalLightComponent.h"
#include "Object/Object.h"

DEFINE_CLASS(ADirectionalLightActor, ALight)
REGISTER_FACTORY(ADirectionalLightActor)

void ADirectionalLightActor::InitDefaultComponents()
{
    auto* DirectionalLightComponent = AddComponent<UDirectionalLightComponent>();
    SetRootComponent(DirectionalLightComponent);

	auto* BillboardComponent = AddComponent<UBillboardComponent>();
    BillboardComponent->SetTextureName("Asset/Texture/DirectionalLight_64x.png");
    BillboardComponent->SetEditorOnly(true);
    BillboardComponent->AttachToComponent(DirectionalLightComponent);
}

FVector ADirectionalLightActor::GetLightDirection() const
{
	UDirectionalLightComponent* DirLightComp = Cast<UDirectionalLightComponent>(GetLightComponent());
    return DirLightComp ? DirLightComp->GetLightDirection() : FVector(1, 0, 0);
}
