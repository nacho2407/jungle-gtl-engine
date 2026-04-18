#include "SpotLightComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Render/Proxy/SpotLightSceneProxy.h"

IMPLEMENT_CLASS(USpotLightComponent, UPointLightComponent)

void USpotLightComponent::Serialize(FArchive& Ar)
{
    UPointLightComponent::Serialize(Ar);
    Ar << InnerConeAngle;
    Ar << OuterConeAngle;
}

void USpotLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UPointLightComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "InnerConeAngle", EPropertyType::Float, &InnerConeAngle, 0.0f, 90.0f, 1.0f });
    OutProps.push_back({ "OuterConeAngle", EPropertyType::Float, &OuterConeAngle, 0.0f, 90.0f, 1.0f });
}

FLightSceneProxy* USpotLightComponent::CreateLightSceneProxy()
{
    return new FSpotLightSceneProxy(this);
}