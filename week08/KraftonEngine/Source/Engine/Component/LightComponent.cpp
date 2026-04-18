#include "LightComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(ULightComponent, ULightComponentBase)

void ULightComponent::Serialize(FArchive& Ar)
{
    ULightComponentBase::Serialize(Ar);
}

void ULightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    ULightComponentBase::GetEditableProperties(OutProps);
}

void ULightComponent::PostEditProperty(const char* PropertyName)
{
    ULightComponentBase::PostEditProperty(PropertyName);
}
