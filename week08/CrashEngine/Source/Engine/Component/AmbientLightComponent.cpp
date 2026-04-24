// 컴포넌트 영역의 세부 동작을 구현합니다.
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

FLightProxy* UAmbientLightComponent::CreateLightProxy()
{
    return new FAmbientLightSceneProxy(this);
}



