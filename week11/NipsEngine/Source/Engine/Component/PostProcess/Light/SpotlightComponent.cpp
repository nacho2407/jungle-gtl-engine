#include "SpotlightComponent.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(USpotlightComponent, UPointLightComponent)
REGISTER_FACTORY(USpotlightComponent)

void USpotlightComponent::PostDuplicate(UObject* Original)
{
    UPointLightComponent::PostDuplicate(Original);
}

void USpotlightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UPointLightComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Spotlight Direction", EPropertyType::Vec3, &Direction });
    OutProps.push_back({ "Inner Cone Angle", EPropertyType::Float, &InnerConeAngle });
    OutProps.push_back({ "Outer Cone Angle", EPropertyType::Float, &OuterConeAngle });
}