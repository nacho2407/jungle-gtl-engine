#include "LightComponentBase.h"
#include "Serialization/Archive.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_ABSTRACT_CLASS(ULightComponentBase, USceneComponent)

void ULightComponentBase::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USceneComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Intensity",EPropertyType::Float,&Intensity,0.0f,50.f,0.05f });
	OutProps.push_back({ "Color",EPropertyType::Color4,&LightColor });
	OutProps.push_back({ "Visible",EPropertyType::Bool,&bVisible });
}

void ULightComponentBase::Serialize(FArchive& Ar)
{
	USceneComponent::Serialize(Ar);
	Ar << Intensity;
	Ar << LightColor;
	Ar << bVisible;
}
