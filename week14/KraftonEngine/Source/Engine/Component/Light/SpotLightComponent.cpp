#include "SpotLightComponent.h"
#include "Engine/Serialization/Archive.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include <cmath>

IMPLEMENT_CLASS(USpotLightComponent, UPointLightComponent)

void USpotLightComponent::ContributeSelectedVisuals(FScene& Scene) const
{
	const FVector Apex = GetWorldLocation();
	const FVector Forward = GetForwardVector();
	const FVector Right = GetRightVector();
	const FVector Up = GetUpVector();
	const float ClampedOuterAngle = FMath::Clamp(OuterConeAngle, 0.0f, 89.0f);
	const float ClampedInnerAngle = FMath::Clamp(InnerConeAngle, 0.0f, ClampedOuterAngle);
	const float ConeLength = AttenuationRadius;

	Scene.AddDebugLine(Apex, Apex + Forward * ConeLength, FColor::White());

	float AngleRadInner = InnerConeAngle * FMath::DegToRad;
	FVector FirstDirection = (Forward + Right * tanf(AngleRadInner)).Normalized() * ConeLength;
	FVector FirstPrev = FirstDirection;
	float AngleRadOuter = OuterConeAngle * FMath::DegToRad;
	FVector SecondDirection = (Forward + Right * tanf(AngleRadOuter)).Normalized() * ConeLength;
	FVector SecondPrev = SecondDirection;
	for (float i = 0.f; i < 2 * FMath::Pi; i += 0.3)
	{
		FQuat Rotation = FQuat::FromAxisAngle(Forward, i);
		FVector ResInner = Rotation.RotateVector(FirstDirection);
		FVector ResOuter = Rotation.RotateVector(SecondDirection);
		Scene.AddDebugLine(Apex, Apex + ResInner, FColor::Green());
		Scene.AddDebugLine(FirstPrev, Apex + ResInner, FColor::Green());
		Scene.AddDebugLine(Apex, Apex + ResOuter, FColor::Yellow());
		Scene.AddDebugLine(SecondPrev, Apex + ResOuter, FColor::Yellow());
		FirstPrev = Apex + ResInner;
		SecondPrev = Apex + ResOuter;
	}
	//Scene.AddDebugLine(FirstPrev, FirstDirection, FColor::Green());
	//Scene.AddDebugLine(SecondPrev, SecondDirection, FColor::Yellow());
}

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

	World->GetScene().GetEnvironment().AddSpotLight(this, Params);
}

void USpotLightComponent::DestroyFromScene()
{
	if (!Owner) return;

	UWorld* World = Owner->GetWorld();
	if (!World) return;

	World->GetScene().GetEnvironment().RemoveSpotLight(this);
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
