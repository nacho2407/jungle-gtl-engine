#include "SpotlightComponent.h"

#include <algorithm>

#include "Object/ObjectFactory.h"

DEFINE_CLASS(USpotLightComponent, UPointLightComponent)
REGISTER_FACTORY(USpotLightComponent)

namespace
{
	float ClampConeAngle(float Angle)
	{
		return std::clamp(Angle, 0.0f, 180.0f);
	}
}

void USpotLightComponent::SetInnerConeAngle(float InAngle)
{
	InnerConeAngle = ClampConeAngle(InAngle);
	OuterConeAngle = std::max(OuterConeAngle, InnerConeAngle);
}

void USpotLightComponent::SetOuterConeAngle(float InAngle)
{
	OuterConeAngle = ClampConeAngle(InAngle);
	OuterConeAngle = std::max(OuterConeAngle, InnerConeAngle);
}

void USpotLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPointLightComponent::GetEditableProperties(OutProps);

	FPropertyDescriptor InnerConeProp{ "Inner Cone Angle", EPropertyType::Float, &InnerConeAngle, 0.0f, 180.0f, 1.0f };
	InnerConeProp.FloatSetter = [this](float InAngle)
	{
		SetInnerConeAngle(InAngle);
	};
	OutProps.push_back(InnerConeProp);

	FPropertyDescriptor OuterConeProp{ "Outer Cone Angle", EPropertyType::Float, &OuterConeAngle, 0.0f, 180.0f, 1.0f };
	OuterConeProp.FloatSetter = [this](float InAngle)
	{
		SetOuterConeAngle(InAngle);
	};
	OutProps.push_back(OuterConeProp);
}

void USpotLightComponent::Serialize(FArchive& Ar)
{
	UPointLightComponent::Serialize(Ar);
	Ar << InnerConeAngle;
	Ar << OuterConeAngle;

	InnerConeAngle = ClampConeAngle(InnerConeAngle);
	OuterConeAngle = std::max(ClampConeAngle(OuterConeAngle), InnerConeAngle);
}
