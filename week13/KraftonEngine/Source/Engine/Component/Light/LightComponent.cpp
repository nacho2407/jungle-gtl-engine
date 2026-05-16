#include "Component/Light/LightComponent.h"
#include "Serialization/Archive.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS_WITH_PROPERTIES(ULightComponent, ULightComponentBase)
HIDE_FROM_COMPONENT_LIST(ULightComponent)

void ULightComponent::Serialize(FArchive& Ar)
{
	ULightComponentBase::Serialize(Ar);
}

void ULightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	ULightComponentBase::GetEditableProperties(OutProps);
}
