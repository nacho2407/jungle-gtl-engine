// 컴포넌트 영역의 세부 동작을 구현합니다.
#include "SpotLightComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Render/Scene/Proxies/Light/SpotLightSceneProxy.h"

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

FLightProxy* USpotLightComponent::CreateLightProxy()
{
    return new FSpotLightSceneProxy(this);
}



