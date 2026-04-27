// 컴포넌트 영역의 세부 동작을 구현합니다.
#include "LightComponentBase.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <algorithm>

IMPLEMENT_ABSTRACT_CLASS(ULightComponentBase, USceneComponent)

// 조명은 일반적으로 Tick을 필요로 하지 않으므로 bTickEnable을 꺼 둔다.
ULightComponentBase::ULightComponentBase()
{
    bTickEnable = false;
}

void ULightComponentBase::Serialize(FArchive& Ar)
{
    USceneComponent::Serialize(Ar);
    Ar << Intensity;
    Ar << LightColor;
    Ar << bAffectsWorld;
    Ar << bCastShadows;
    Ar << ShadowBias;
    Ar << ShadowSlopeBias;
    Ar << ShadowNormalBias;
}

void ULightComponentBase::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    USceneComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Intensity", EPropertyType::Float, &Intensity, 0.0f, 20.0f, 0.1f });
    OutProps.push_back({ "LightColor", EPropertyType::Color4, &LightColor });
    OutProps.push_back({ "bAffectsWorld", EPropertyType::Bool, &bAffectsWorld });
    OutProps.push_back({ "bCastShadows", EPropertyType::Bool, &bCastShadows });
    OutProps.push_back({ "Bias", EPropertyType::Float, &ShadowBias, 0.0f, 0.1f, 0.0001f });
    OutProps.push_back({ "Slope Bias", EPropertyType::Float, &ShadowSlopeBias, 0.0f, 8.0f, 0.01f });
    OutProps.push_back({ "Normal Bias", EPropertyType::Float, &ShadowNormalBias, 0.0f, 8.0f, 0.01f });
}

void ULightComponentBase::PostEditProperty(const char* PropertyName)
{
    ShadowBias = std::max(0.0f, ShadowBias);
    ShadowSlopeBias = std::max(0.0f, ShadowSlopeBias);
    ShadowNormalBias = std::max(0.0f, ShadowNormalBias);
    USceneComponent::PostEditProperty(PropertyName);
}
