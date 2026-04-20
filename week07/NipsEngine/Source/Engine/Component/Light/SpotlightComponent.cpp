#include "SpotlightComponent.h"

#include "Object/ObjectFactory.h"

DEFINE_CLASS(USpotLightComponent, UPointLightComponent)
REGISTER_FACTORY(USpotLightComponent)

void USpotLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPointLightComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Inner Cone Angle", EPropertyType::Float, &InnerConeAngle, 0.0f, 180.0f, 1.0f });
	OutProps.push_back({ "Outer Cone Angle", EPropertyType::Float, &OuterConeAngle, 0.0f, 180.0f, 1.0f });
}

void USpotLightComponent::Serialize(FArchive& Ar)
{
	UPointLightComponent::Serialize(Ar);
	Ar << InnerConeAngle;
	Ar << OuterConeAngle;
}
