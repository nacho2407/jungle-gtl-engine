// 컴포넌트 영역의 세부 동작을 구현합니다.
#include "LightComponentBase.h"

#include <algorithm>

#include "Component/LightComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_ABSTRACT_COMPONENT_CLASS(ULightComponentBase, USceneComponent)

namespace
{
void MarkLightDirtyIfPossible(ULightComponentBase* Base)
{
    if (ULightComponent* Light = Cast<ULightComponent>(Base))
    {
        Light->MarkRenderStateDirty();
    }
}
} // namespace

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
    Ar << ShadowResolution;
    Ar << ShadowBias;
    Ar << ShadowSlopeBias;
    Ar << ShadowNormalBias;
    Ar << ShadowSharpen;
    Ar << ShadowESMExponent;
}

void ULightComponentBase::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    USceneComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Intensity", EPropertyType::Float, &Intensity, 0.0f, 20.0f, 0.1f });
    OutProps.push_back({ "LightColor", EPropertyType::Color4, &LightColor });
    OutProps.push_back({ "bAffectsWorld", EPropertyType::Bool, &bAffectsWorld });
    OutProps.push_back({ "Cast Shadows", EPropertyType::Bool, &bCastShadows });
    OutProps.push_back({ "Depth Bias", EPropertyType::Float, &ShadowBias, 0.0f, 0.1f, 0.0001f });
    OutProps.push_back({ "Slope Bias", EPropertyType::Float, &ShadowSlopeBias, 0.0f, 8.0f, 0.01f });
    OutProps.push_back({ "Normal Bias", EPropertyType::Float, &ShadowNormalBias, 0.0f, 8.0f, 0.01f });
    OutProps.push_back({ "Shadow Sharpen", EPropertyType::Float, &ShadowSharpen, 0.0f, 0.99f, 0.01f });
    OutProps.push_back({ "ESM Exponent", EPropertyType::Float, &ShadowESMExponent, 0.01f, 100.0f, 0.1f });
}

void ULightComponentBase::PostEditProperty(const char* PropertyName)
{
    ShadowResolution = RoundShadowResolutionToTier(GetShadowResolution());
    ShadowBias = std::max(0.0f, ShadowBias);
    ShadowSlopeBias = std::max(0.0f, ShadowSlopeBias);
    ShadowNormalBias = std::max(0.0f, ShadowNormalBias);
    ShadowSharpen = std::clamp(ShadowSharpen, 0.0f, 0.99f);
    ShadowESMExponent = std::max(0.01f, ShadowESMExponent);
    USceneComponent::PostEditProperty(PropertyName);
}

void ULightComponentBase::SetIntensity(float InIntensity)
{
    Intensity = std::max(0.0f, InIntensity);
    MarkLightDirtyIfPossible(this);
}

void ULightComponentBase::SetLightColor(const FVector4& InColor)
{
    LightColor = InColor;
    MarkLightDirtyIfPossible(this);
}

void ULightComponentBase::SetAffectsWorld(bool bInAffectsWorld)
{
    bAffectsWorld = bInAffectsWorld;
    MarkLightDirtyIfPossible(this);
}

void ULightComponentBase::SetCastShadows(bool bInCastShadows)
{
    bCastShadows = bInCastShadows;
    MarkLightDirtyIfPossible(this);
}
