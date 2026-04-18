#include "DirectionalLightComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Render/Proxy/LightSceneProxy.h"

IMPLEMENT_CLASS(UDirectionalLightComponent, ULightComponent)

UDirectionalLightComponent::UDirectionalLightComponent()
{
    // UE5 Directional Light Default Intensity
    Intensity = 10.0f;
}

void UDirectionalLightComponent::Serialize(FArchive& Ar)
{
    ULightComponent::Serialize(Ar);
    Ar << bUseTemperature;
    Ar << Temperature;
}

void UDirectionalLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    ULightComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "bUseTemperature", EPropertyType::Bool, &bUseTemperature });
    OutProps.push_back({ "Temperature", EPropertyType::Float, &Temperature, 1700.0f, 12000.0f, 10.0f });
}

void UDirectionalLightComponent::PostEditProperty(const char* PropertyName)
{
    ULightComponent::PostEditProperty(PropertyName);
}

FLightSceneProxy* UDirectionalLightComponent::CreateLightSceneProxy()
{
    FLightSceneProxy* Proxy = new FLightSceneProxy(this);
    Proxy->LightConstants.LightType = static_cast<uint32>(ELightType::Directional);
    return Proxy;
}
