#include "AmbientLight.h"

#include "Component/BillboardComponent.h"
#include "Component/Light/AmbientLightComponent.h"
#include "Object/Object.h"

DEFINE_CLASS(AmbientLight, ALight)
REGISTER_FACTORY(AmbientLight)

void AmbientLight::InitDefaultComponents()
{
    auto* AmbientLightComponent = AddComponent<UAmbientLightComponent>();
    SetRootComponent(AmbientLightComponent);

	auto* BillboardComponent = AddComponent<UBillboardComponent>();
    BillboardComponent->SetTextureName("Asset/Texture/SkyLight_64x.png");
    BillboardComponent->SetEditorOnly(true);
    BillboardComponent->AttachToComponent(AmbientLightComponent);
}
