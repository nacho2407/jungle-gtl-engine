// 컴포넌트 영역의 세부 동작을 구현합니다.
#include "DirectionalLightComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Render/Scene/Proxies/Light/DirectionalLightSceneProxy.h"

#include <algorithm>

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
    Ar << CascadeCount;
    Ar << DynamicShadowDistance;
    Ar << CascadeDistribution;
}

void UDirectionalLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    ULightComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Cascade Count", EPropertyType::Int, &CascadeCount, 1.0f, 4.0f, 1.0f });
    OutProps.push_back({ "CSM Max Distance", EPropertyType::Float, &DynamicShadowDistance, 100.0f, 20000.0f, 10.0f });
    OutProps.push_back({ "Cascade Distribution", EPropertyType::Float, &CascadeDistribution, 0.1f, 4.0f, 0.01f });
}

void UDirectionalLightComponent::PostEditProperty(const char* PropertyName)
{
    CascadeCount = std::clamp(CascadeCount, 1, 4);
    DynamicShadowDistance = std::max(100.0f, DynamicShadowDistance);
    CascadeDistribution = std::clamp(CascadeDistribution, 0.1f, 4.0f);
    ULightComponent::PostEditProperty(PropertyName);
}

FLightProxy* UDirectionalLightComponent::CreateLightProxy()
{
    return new FDirectionalLightSceneProxy(this);
}



