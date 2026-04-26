#include "PointLightComponent.h"
#include "Engine/Serialization/Archive.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Render/Types/LightFrustumUtils.h"

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

IMPLEMENT_CLASS(UPointLightComponent, ULightComponent)

void UPointLightComponent::ContributeSelectedVisuals(FScene& Scene) const
{
	const FVector Center = GetWorldLocation();
	constexpr int32 Segments = 24;

	AddWireCircle(Scene, Center, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), AttenuationRadius, Segments, FColor::Yellow());
	AddWireCircle(Scene, Center, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), AttenuationRadius, Segments, FColor::Yellow());
	AddWireCircle(Scene, Center, FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), AttenuationRadius, Segments, FColor::Yellow());
}

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
	Params.bCastShadows = bCastShadows;

	World->GetScene().GetEnvironment().AddPointLight(this, Params);
}

void UPointLightComponent::DestroyFromScene()
{
	if (!Owner) return;
	UWorld* World = Owner->GetWorld();
	if (!World) return;
	World->GetScene().GetEnvironment().RemovePointLight(this);
}

void UPointLightComponent::Serialize(FArchive& Ar)
{
	ULightComponent::Serialize(Ar);
	Ar << AttenuationRadius;
	Ar << LightFalloffExponent;
}

// TODO: Camera Parameter의 존재 이유?
bool UPointLightComponent::GetLightViewProj(FLightViewProjResult& OutResult, const UCameraComponent* Camera, int32 FaceIndex) const
{
	FPointLightParams Params;
	Params.Position = GetWorldLocation();
	Params.AttenuationRadius = AttenuationRadius;

	auto VP = FLightFrustumUtils::BuildPointLightFaceViewProj(Params, FaceIndex);
	OutResult.View = VP.View;
	OutResult.Proj = VP.Proj;
	OutResult.bIsOrtho = false;
	return true;
}

void UPointLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	ULightComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "AttenuationRadius",EPropertyType::Float,&AttenuationRadius,0.05f,1000.f,0.01f });
	OutProps.push_back({ "LightFalloffExponent",EPropertyType::Float,&LightFalloffExponent,0.05f,10.f,0.01f });
}
