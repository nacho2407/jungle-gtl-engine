#include "LightComponentBase.h"

#include "Object/ObjectFactory.h"

DEFINE_CLASS(ULightComponentBase, USceneComponent)
// ULightComponentBase는 추상 클래스이므로 Factory 등록은 필요하지 않음!

void ULightComponentBase::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    USceneComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Visible", EPropertyType::Bool, &bVisible });
    OutProps.push_back({ "Intensity", EPropertyType::Float, &Intensity, 0.0f, 100.0f, 1.0f }); // 색 변화 보면서 세부 조정
    OutProps.push_back({ "Light Color", EPropertyType::Color, &LightColor });
}

void ULightComponentBase::Serialize(FArchive& Ar)
{
    USceneComponent::Serialize(Ar);
    Ar << bVisible;
    Ar << Intensity;
    Ar << LightColor;
}
