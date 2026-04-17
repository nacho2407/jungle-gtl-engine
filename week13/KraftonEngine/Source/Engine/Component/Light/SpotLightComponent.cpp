#include "SpotLightComponent.h"
#include "Engine/Serialization/Archive.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include <cmath>

namespace
{
	void AddWireCircle(FScene& Scene, const FVector& Center, const FVector& AxisA, const FVector& AxisB, float Radius, int32 Segments, const FColor& Color)
	{
		if (Radius <= 0.0f || Segments < 3)
		{
			return;
		}

		const float Step = 2.0f * FMath::Pi / static_cast<float>(Segments);
		FVector Prev = Center + AxisA * Radius;

		for (int32 Index = 1; Index <= Segments; ++Index)
		{
			const float Angle = Step * static_cast<float>(Index);
			const FVector Next = Center + (AxisA * cosf(Angle) + AxisB * sinf(Angle)) * Radius;
			Scene.AddDebugLine(Prev, Next, Color);
			Prev = Next;
		}
	}
}

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

	const auto AddCone = [&](float AngleDeg, const FColor& Color)
	{
		if (ConeLength <= 0.0f)
		{
			return;
		}

		const float AngleRad = AngleDeg * FMath::DegToRad;
		const float RingRadius = tanf(AngleRad) * ConeLength;
		const FVector RingCenter = Apex + Forward * ConeLength;
		constexpr int32 Segments = 24;

		AddWireCircle(Scene, RingCenter, Right, Up, RingRadius, Segments, Color);

		const FVector RimOffsets[4] =
		{
			Right * RingRadius,
			Right * -RingRadius,
			Up * RingRadius,
			Up * -RingRadius
		};

		for (const FVector& Offset : RimOffsets)
		{
			Scene.AddDebugLine(Apex, RingCenter + Offset, Color);
		}
	};

	AddCone(ClampedInnerAngle, FColor::Green());
	AddCone(ClampedOuterAngle, FColor::Yellow());
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
