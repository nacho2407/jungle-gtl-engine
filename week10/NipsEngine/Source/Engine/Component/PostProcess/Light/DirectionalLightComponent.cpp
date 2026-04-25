#include "DirectionalLightComponent.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UDirectionalLightComponent, ULightComponent)
REGISTER_FACTORY(UDirectionalLightComponent)

void UDirectionalLightComponent::PostDuplicate(UObject* Original)
{
    ULightComponentBase::PostDuplicate(Original);
    UDirectionalLightComponent* Orig = Cast<UDirectionalLightComponent>(Original);
}

void UDirectionalLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	ULightComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Shadow Resolution Scale", EPropertyType::Float, &ShadowResolutionScale });
	OutProps.push_back({ "Shadow Bias", EPropertyType::Float, &ShadowBias });
	OutProps.push_back({ "Shadow Slope Bias", EPropertyType::Float, &ShadowSlopeBias });
	OutProps.push_back({ "Shadow Sharpen", EPropertyType::Float, &ShadowSharpen });
}
