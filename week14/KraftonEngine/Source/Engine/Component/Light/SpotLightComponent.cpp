#include "SpotLightComponent.h"
#include "Engine/Serialization/Archive.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Render/Types/LightFrustumUtils.h"
#include <cmath>

IMPLEMENT_CLASS(USpotLightComponent, UPointLightComponent)

void USpotLightComponent::ContributeSelectedVisuals(FScene& Scene) const
{
	const FVector Apex = GetWorldLocation();
	const FVector Forward = GetForwardVector();
	const FVector Right = GetRightVector();
	const float ClampedOuterAngle = FMath::Clamp(OuterConeAngle, 0.0f, 89.0f);
	const float ClampedInnerAngle = FMath::Clamp(InnerConeAngle, 0.0f, ClampedOuterAngle);
	const float ConeLength = AttenuationRadius;

	Scene.AddDebugLine(Apex, Apex + Forward * ConeLength, FColor::White());

	const float AngleRadInner = ClampedInnerAngle * FMath::DegToRad;
	const float AngleRadOuter = ClampedOuterAngle * FMath::DegToRad;
	const FVector InnerEdge = Forward * ConeLength + Right * (tanf(AngleRadInner) * ConeLength);
	const FVector OuterEdge = Forward * ConeLength + Right * (tanf(AngleRadOuter) * ConeLength);

	constexpr int32 SegmentCount = 24;
	const float SegmentStep = 2.0f * FMath::Pi / static_cast<float>(SegmentCount);
	FVector PreviousInnerPoint = Apex + InnerEdge;
	FVector PreviousOuterPoint = Apex + OuterEdge;

	for (int32 SegmentIndex = 1; SegmentIndex <= SegmentCount; ++SegmentIndex)
	{
		const float Angle = SegmentStep * static_cast<float>(SegmentIndex);
		const FQuat Rotation = FQuat::FromAxisAngle(Forward, Angle);
		const FVector InnerPoint = Apex + Rotation.RotateVector(InnerEdge);
		const FVector OuterPoint = Apex + Rotation.RotateVector(OuterEdge);

		Scene.AddDebugLine(Apex, InnerPoint, FColor::Green());
		Scene.AddDebugLine(PreviousInnerPoint, InnerPoint, FColor::Green());
		Scene.AddDebugLine(Apex, OuterPoint, FColor::Yellow());
		Scene.AddDebugLine(PreviousOuterPoint, OuterPoint, FColor::Yellow());

		PreviousInnerPoint = InnerPoint;
		PreviousOuterPoint = OuterPoint;
	}
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
	Params.bCastShadows = bCastShadows;
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

bool USpotLightComponent::GetLightViewProj(FLightViewProjResult& OutResult, const UCameraComponent* Camera, int32 FaceIndex) const
{
	FSpotLightParams Params;
	Params.Position = GetWorldLocation();
	Params.Direction = GetForwardVector();
	Params.AttenuationRadius = AttenuationRadius;
	float ClampedOuter = FMath::Clamp(OuterConeAngle, 0.0f, 89.0f);
	Params.OuterConeCos = cosf(ClampedOuter * FMath::DegToRad);

	auto VP = FLightFrustumUtils::BuildSpotLightViewProj(Params);
	OutResult.View = VP.View;
	OutResult.Proj = VP.Proj;
	OutResult.bIsOrtho = false;
	return true;
}

void USpotLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPointLightComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "InnerConeAngle", EPropertyType::Float, &InnerConeAngle, 0.0f, 89.0f, 0.1f });
	OutProps.push_back({ "OuterConeAngle", EPropertyType::Float, &OuterConeAngle, 0.0f, 89.0f, 0.1f });
}
