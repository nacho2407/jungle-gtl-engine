// 컴포넌트 영역의 세부 동작을 구현합니다.
#include "PointLightComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Render/Scene/Proxies/Light/PointLightSceneProxy.h"

IMPLEMENT_CLASS(UPointLightComponent, ULightComponent)

void UPointLightComponent::Serialize(FArchive& Ar)
{
    ULightComponent::Serialize(Ar);
    Ar << AttenuationRadius;
    Ar << LightFalloffExponent;
    Ar << bUseInverseSquaredFalloff;
}

void UPointLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    ULightComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "AttenuationRadius", EPropertyType::Float, &AttenuationRadius, 0.0f, 10000.0f, 10.0f });
    OutProps.push_back({ "LightFalloffExponent", EPropertyType::Float, &LightFalloffExponent, 0.0f, 20.0f, 0.1f });
    OutProps.push_back({ "UseInverseSquaredFalloff", EPropertyType::Bool, &bUseInverseSquaredFalloff });
}

void UPointLightComponent::PostEditProperty(const char* PropertyName)
{
    ULightComponent::PostEditProperty(PropertyName);
}

FLightProxy* UPointLightComponent::CreateLightProxy()
{
    return new FPointLightSceneProxy(this);
}



