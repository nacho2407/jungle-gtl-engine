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
}

void ULightComponentBase::Serialize(FArchive& Ar)
{
	USceneComponent::Serialize(Ar);
	Ar << "Color" << LightColor;
	Ar << "Intensity" << Intensity;
}
