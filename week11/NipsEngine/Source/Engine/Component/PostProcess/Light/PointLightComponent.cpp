#include "PointLightComponent.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UPointLightComponent, ULightComponent)
REGISTER_FACTORY(UPointLightComponent)

void UPointLightComponent::PostDuplicate(UObject* Original)
{
    ULightComponentBase::PostDuplicate(Original);
}

void UPointLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    ULightComponentBase::GetEditableProperties(OutProps);
    OutProps.push_back({ "Attenuation Radius", EPropertyType::Float, &AttenuationRadius });
    OutProps.push_back({ "Light Falloff", EPropertyType::Float, &LightFalloffExponent });
}

void UPointLightComponent::Serialize(FArchive& Ar)
{
	ULightComponentBase::Serialize(Ar);
	Ar << "AttenuationRadius" << AttenuationRadius;
	Ar << "LightFalloffExponent" << LightFalloffExponent;
}
