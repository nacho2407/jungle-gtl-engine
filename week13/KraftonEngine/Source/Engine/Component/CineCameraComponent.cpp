#include "Component/CineCameraComponent.h"

#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS_WITH_PROPERTIES(UCineCameraComponent, UCameraComponent)

BEGIN_PROPERTY_REGISTRATION(UCineCameraComponent)
	EDIT_PROPERTY(UCineCameraComponent, Letterbox.bEnabled, "Enable Letterbox", EPropertyType::Bool, "Cinematic")
	EDIT_PROPERTY_RANGE(UCineCameraComponent, Letterbox.Amount, "Letterbox Amount", EPropertyType::Float, "Cinematic", 0.0f, 1.0f, 0.01f)
	EDIT_PROPERTY_RANGE(UCineCameraComponent, Letterbox.Thickness, "Letterbox Thickness", EPropertyType::Float, "Cinematic", 0.0f, 0.5f, 0.01f)
	EDIT_PROPERTY(UCineCameraComponent, Letterbox.Color, "Letterbox Color", EPropertyType::Color4, "Cinematic")
END_PROPERTY_REGISTRATION()

void UCineCameraComponent::Serialize(FArchive& Ar)
{
	UCameraComponent::Serialize(Ar);
	Ar << Letterbox.bEnabled;
	Ar << Letterbox.Amount;
	Ar << Letterbox.Thickness;
	Ar << Letterbox.Color;
}

void UCineCameraComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UCameraComponent::GetEditableProperties(OutProps);
}
