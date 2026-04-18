#include "LightComponentBase.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

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
}

void ULightComponentBase::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    USceneComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Intensity", EPropertyType::Float, &Intensity, 0.0f, 20.0f, 0.1f });
    OutProps.push_back({ "LightColor", EPropertyType::Color4, &LightColor });
    OutProps.push_back({ "bAffectsWorld", EPropertyType::Bool, &bAffectsWorld });
    OutProps.push_back({ "bCastShadows", EPropertyType::Bool, &bCastShadows });
}

void ULightComponentBase::PostEditProperty(const char* PropertyName)
{
    USceneComponent::PostEditProperty(PropertyName);
}