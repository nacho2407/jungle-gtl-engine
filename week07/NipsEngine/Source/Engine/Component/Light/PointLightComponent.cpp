#include "PointLightComponent.h"

#include "Object/ObjectFactory.h"

DEFINE_CLASS(UPointLightComponent, ULightComponent)
REGISTER_FACTORY(UPointLightComponent)

void UPointLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	ULightComponentBase::GetEditableProperties(OutProps);
	OutProps.push_back({ "Attenuation Radius", EPropertyType::Float, &AttenuationRadius, 0.0f, 1000.0f, 0.1f });
	OutProps.push_back({ "Light Falloff Exponent", EPropertyType::Float, &LightFalloffExponent, 0.0f, 10.0f, 0.1f });
}

void UPointLightComponent::Serialize(FArchive& Ar)
{
	ULightComponentBase::Serialize(Ar);
	Ar << AttenuationRadius;
	Ar << LightFalloffExponent;
}
