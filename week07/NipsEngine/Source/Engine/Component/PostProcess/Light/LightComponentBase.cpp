#include "LightComponentBase.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(ULightComponentBase, USceneComponent)
REGISTER_FACTORY(ULightComponentBase)

void ULightComponentBase::PostDuplicate(UObject* Original)
{
    USceneComponent::PostDuplicate(Original);
    const ULightComponentBase* Orig = Cast<ULightComponentBase>(Original);

    LightColor = Orig->LightColor;
}

void ULightComponentBase::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    USceneComponent::GetEditableProperties(OutProps);

    OutProps.push_back({ "Color", EPropertyType::Color, &LightColor });
    OutProps.push_back({ "Intensity", EPropertyType::Float, &Intensity });
    OutProps.push_back({ "Affects World", EPropertyType::Bool, &bAffectsWorld });
}