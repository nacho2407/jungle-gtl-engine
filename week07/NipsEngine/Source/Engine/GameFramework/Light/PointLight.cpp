#include "PointLight.h"

#include "Component/BillboardComponent.h"
#include "Component/Light/PointLightComponent.h"
#include "Object/Object.h"

DEFINE_CLASS(APointLightActor, ALight)
REGISTER_FACTORY(APointLightActor)

UPointLightComponent* APointLightActor::GetPointLightComponent() const
{
    return Cast<UPointLightComponent>(GetLightComponent());
}

void APointLightActor::InitDefaultComponents()
{
    auto* PointLightComponent = AddComponent<UPointLightComponent>();
    SetRootComponent(PointLightComponent);

    auto* BillboardComponent = AddComponent<UBillboardComponent>();
    BillboardComponent->SetTextureName("Asset/Texture/PointLight_64x.png");
    BillboardComponent->SetEditorOnly(true);
    BillboardComponent->AttachToComponent(PointLightComponent);
}

float APointLightActor::GetAttenuationRadius() const
{
    const auto* PointLightComponent = GetPointLightComponent();
    return PointLightComponent ? PointLightComponent->GetAttenuationRadius() : 0.0f;
}

float APointLightActor::GetLightFalloffExponent() const
{
    const auto* PointLightComponent = GetPointLightComponent();
    return PointLightComponent ? PointLightComponent->GetLightFalloffExponent() : 0.0f;
}
