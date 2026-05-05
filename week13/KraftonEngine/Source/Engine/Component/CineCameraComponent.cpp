#include "Component/CineCameraComponent.h"

#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UCineCameraComponent, UCameraComponent)

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
	OutProps.push_back({ "Enable Letterbox", EPropertyType::Bool, "Cinematic", &Letterbox.bEnabled });
	OutProps.push_back({ "Letterbox Amount", EPropertyType::Float, "Cinematic", &Letterbox.Amount, 0.0f, 1.0f, 0.01f });
	OutProps.push_back({ "Letterbox Thickness", EPropertyType::Float, "Cinematic", &Letterbox.Thickness, 0.0f, 0.5f, 0.01f });
	OutProps.push_back({ "Letterbox Color", EPropertyType::Color4, "Cinematic", &Letterbox.Color });
}
