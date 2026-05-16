#include "Component/Light/LightComponent.h"
#include "Serialization/Archive.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS_WITH_PROPERTIES(ULightComponent, ULightComponentBase)
HIDE_FROM_COMPONENT_LIST(ULightComponent)

BEGIN_PROPERTY_REGISTRATION(ULightComponent)
	EDIT_PROPERTY_RANGE(ULightComponent, ShadowResolutionScale, "Shadow Resolution Scale", EPropertyType::Float, "Shadow", 0.1f, 4.0f, 0.1f)
	EDIT_PROPERTY_RANGE(ULightComponent, ShadowBias, "Shadow Bias", EPropertyType::Float, "Shadow", -0.2f, 0.2f, 0.0001f)
	EDIT_PROPERTY_RANGE(ULightComponent, ShadowSlopeBias, "Shadow Slope Bias", EPropertyType::Float, "Shadow", -0.2f, 0.2f, 0.0001f)
	EDIT_PROPERTY_RANGE(ULightComponent, ShadowNormalBias, "Shadow Normal Bias", EPropertyType::Float, "Shadow", -0.2f, 0.2f, 0.0001f)
	EDIT_PROPERTY_RANGE(ULightComponent, ShadowSharpen, "Shadow Sharpen", EPropertyType::Float, "Shadow", 0.0f, 1.0f, 0.05f)
END_PROPERTY_REGISTRATION()

void ULightComponent::Serialize(FArchive& Ar)
{
	ULightComponentBase::Serialize(Ar);
	Ar << ShadowResolutionScale;
	Ar << ShadowBias;
	Ar << ShadowSlopeBias;
	Ar << ShadowNormalBias;
	Ar << ShadowSharpen;
}

void ULightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	ULightComponentBase::GetEditableProperties(OutProps);
}
