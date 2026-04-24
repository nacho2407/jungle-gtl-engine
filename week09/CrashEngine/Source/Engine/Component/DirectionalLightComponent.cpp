// 컴포넌트 영역의 세부 동작을 구현합니다.
#include "DirectionalLightComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Render/Scene/Proxies/Light/DirectionalLightSceneProxy.h"

IMPLEMENT_CLASS(UDirectionalLightComponent, ULightComponent)

UDirectionalLightComponent::UDirectionalLightComponent()
{
    // UE5 Directional Light Default Intensity
    Intensity = 2.5f;
}

void UDirectionalLightComponent::Serialize(FArchive& Ar)
{
    ULightComponent::Serialize(Ar);
    Ar << Direction;
}

void UDirectionalLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    ULightComponent::GetEditableProperties(OutProps);
}

void UDirectionalLightComponent::PostEditProperty(const char* PropertyName)
{
    ULightComponent::PostEditProperty(PropertyName);
}

FLightProxy* UDirectionalLightComponent::CreateLightProxy()
{
    return new FDirectionalLightSceneProxy(this);
}



