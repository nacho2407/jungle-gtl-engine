#include "FireballComponent.h"
#include "Render/Proxy/FireballSceneProxy.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UFireballComponent, UPrimitiveComponent)

void UFireballComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);
	Ar << Intensity;
	Ar << Radius;
	Ar << RadiusFalloff;
	Ar << Color;
}

void UFireballComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	OutProps.push_back({ "Intensity", EPropertyType::Float, &Intensity, 0.0f, 10.0f, 0.1f });
	OutProps.push_back({ "Radius", EPropertyType::Float, &Radius, 0.0f, 1000.0f, 1.0f });
	OutProps.push_back({ "Radius Falloff", EPropertyType::Float, &RadiusFalloff, 0.1f, 10.0f, 0.1f });
	OutProps.push_back({ "Color", EPropertyType::Vec4, &Color });
}

FPrimitiveSceneProxy* UFireballComponent::CreateSceneProxy()
{
	return new FFireballSceneProxy(this);
}
