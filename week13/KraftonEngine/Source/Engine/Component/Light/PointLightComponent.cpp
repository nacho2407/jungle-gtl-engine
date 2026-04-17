#include "PointLightComponent.h"
#include "Engine/Serialization/Archive.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

IMPLEMENT_CLASS(UPointLightComponent, ULightComponent)

void UPointLightComponent::PushToScene()
{
	if (!Owner) return;
	UWorld* World = Owner->GetWorld();
	if (!World) return;

	FPointLightParams Params;
	Params.AttenuationRadius = AttenuationRadius;
	Params.bVisible = bVisible;
	Params.Intensity = Intensity;
	Params.LightColor = LightColor;
	Params.LightFalloffExponent = LightFalloffExponent;
	Params.LightType = ELightType::Point;
	Params.Position = GetWorldLocation();

	World->GetScene().AddPointLight(this, Params);
}

void UPointLightComponent::DestroyFromScene()
{
	if (!Owner) return;
	UWorld* World = Owner->GetWorld();
	if (!World) return;
	World->GetScene().RemovePointLight(this);
}

void UPointLightComponent::Serialize(FArchive& Ar)
{
	ULightComponent::Serialize(Ar);
	Ar << AttenuationRadius;
	Ar << LightFalloffExponent;
}

void UPointLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	ULightComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "AttenuationRadius",EPropertyType::Float,&AttenuationRadius,0.05f,1000.f,0.01f });
	OutProps.push_back({ "LightFalloffExponent",EPropertyType::Float,&LightFalloffExponent,0.05f,10.f,0.01f });
}
