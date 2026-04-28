#include "PointLightComponent.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UPointLightComponent, ULightComponent)
REGISTER_FACTORY(UPointLightComponent)

void UPointLightComponent::PostDuplicate(UObject* Original)
{
    ULightComponent::PostDuplicate(Original);
}

void UPointLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    ULightComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Attenuation Radius", EPropertyType::Float, &AttenuationRadius });
    OutProps.push_back({ "Light Falloff", EPropertyType::Float, &LightFalloffExponent });
}

void UPointLightComponent::Serialize(FArchive& Ar)
{
	ULightComponent::Serialize(Ar);
	Ar << "AttenuationRadius" << AttenuationRadius;
	Ar << "LightFalloffExponent" << LightFalloffExponent;
}

FMatrix UPointLightComponent::ComputePerspectiveShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
	const TArray<FBoundingBox>* VisibleObjectsBounds) const
{
	return FMatrix::Identity;
}
