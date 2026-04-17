#include "SpotLightComponent.h"
#include "Engine/Serialization/Archive.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include <cmath>

IMPLEMENT_CLASS(USpotLightComponent, UPointLightComponent)

void USpotLightComponent::PushToScene()
{
	if (!Owner) return;

	UWorld* World = Owner->GetWorld();
	if (!World) return;

	const float ClampedOuterAngle = FMath::Clamp(OuterConeAngle, 0.0f, 89.0f);
	const float ClampedInnerAngle = FMath::Clamp(InnerConeAngle, 0.0f, ClampedOuterAngle);

	FSpotLightParams Params;
	Params.AttenuationRadius = AttenuationRadius;
	Params.bVisible = bVisible;
	Params.Intensity = Intensity;
	Params.LightColor = LightColor;
	Params.LightFalloffExponent = LightFalloffExponent;
	Params.LightType = ELightType::Spot;
	Params.Position = GetWorldLocation();
	Params.Direction = GetForwardVector();
	Params.InnerConeCos = std::cos(ClampedInnerAngle * FMath::DegToRad);
	Params.OuterConeCos = std::cos(ClampedOuterAngle * FMath::DegToRad);

	World->GetScene().AddSpotLight(this, Params);
}

void USpotLightComponent::DestroyFromScene()
{
	if (!Owner) return;

	UWorld* World = Owner->GetWorld();
	if (!World) return;

	World->GetScene().RemoveSpotLight(this);
}

void USpotLightComponent::Serialize(FArchive& Ar)
{
	UPointLightComponent::Serialize(Ar);
	Ar << InnerConeAngle;
	Ar << OuterConeAngle;
}

void USpotLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPointLightComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "InnerConeAngle", EPropertyType::Float, &InnerConeAngle, 0.0f, 89.0f, 0.1f });
	OutProps.push_back({ "OuterConeAngle", EPropertyType::Float, &OuterConeAngle, 0.0f, 89.0f, 0.1f });
}
