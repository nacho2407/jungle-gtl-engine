#include "AmbientLightComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Render/Scene/Proxies/Light/AmbientLightSceneProxy.h"

IMPLEMENT_CLASS(UAmbientLightComponent, ULightComponent)

UAmbientLightComponent::UAmbientLightComponent()
{
    Intensity = 0.5f;
}

void UAmbientLightComponent::Serialize(FArchive& Ar)
{
    ULightComponent::Serialize(Ar);
}

void UAmbientLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    ULightComponent::GetEditableProperties(OutProps);
}

void UAmbientLightComponent::PostEditProperty(const char* PropertyName)
{
    ULightComponent::PostEditProperty(PropertyName);
}

FLightSceneProxy* UAmbientLightComponent::CreateLightSceneProxy()
{
    return new FAmbientLightSceneProxy(this);
}
