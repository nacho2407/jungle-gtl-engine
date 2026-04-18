#include "DirectionalLightComponent.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UDirectionalLightComponent, ULightComponent)
REGISTER_FACTORY(UDirectionalLightComponent)

void UDirectionalLightComponent::PostDuplicate(UObject* Original)
{
    ULightComponentBase::PostDuplicate(Original);
    UDirectionalLightComponent* Orig = Cast<UDirectionalLightComponent>(Original);
    LightDirection = Orig->LightDirection;
}

void UDirectionalLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    ULightComponentBase::GetEditableProperties(OutProps);
    OutProps.push_back({ "Direction", EPropertyType::Vec3, &LightDirection });
}