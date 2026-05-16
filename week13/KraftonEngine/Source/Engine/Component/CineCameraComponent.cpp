#include "Component/CineCameraComponent.h"

#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS_WITH_PROPERTIES(UCineCameraComponent, UCameraComponent)

void UCineCameraComponent::Serialize(FArchive& Ar)
{
	UCameraComponent::Serialize(Ar);
}

void UCineCameraComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UCameraComponent::GetEditableProperties(OutProps);
}
