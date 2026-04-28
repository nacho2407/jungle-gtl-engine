#include "Component/Light/LightComponent.h"
#include "Serialization/Archive.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(ULightComponent, ULightComponentBase)
HIDE_FROM_COMPONENT_LIST(ULightComponent)

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
	OutProps.push_back({ "Shadow Resolution Scale", EPropertyType::Float, &ShadowResolutionScale, 0.1f, 4.0f, 0.1f });
	OutProps.push_back({ "Shadow Bias",             EPropertyType::Float, &ShadowBias,            -0.2f, 0.2f, 0.0001f });
	OutProps.push_back({ "Shadow Slope Bias",       EPropertyType::Float, &ShadowSlopeBias,       -0.2f, 0.2f, 0.0001f });
	OutProps.push_back({ "Shadow Normal Bias",      EPropertyType::Float, &ShadowNormalBias,      -0.2f, 0.2f, 0.0001f });
	OutProps.push_back({ "Shadow Sharpen",          EPropertyType::Float, &ShadowSharpen,         0.0f, 1.0f, 0.05f });
}
