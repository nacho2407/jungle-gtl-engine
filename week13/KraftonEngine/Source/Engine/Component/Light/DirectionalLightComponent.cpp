#include "DirectionalLightComponent.h"
#include "Render/Types/GlobalLightParams.h"
#include "Render/Types/LightFrustumUtils.h"
#include "Component/CameraComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Engine/Serialization/Archive.h"
#include <cmath>

IMPLEMENT_CLASS(UDirectionalLightComponent, ULightComponent)

namespace
{
	void AddDirectionalLightArrow(FScene& Scene, const FVector& Origin, const FVector& Direction)
	{
		const FVector Forward = Direction.Normalized();
		if (Forward.Length() <= 0.001f)
		{
			return;
		}

		constexpr float ArrowLength = 2.2f;
		constexpr float HeadLength = 0.55f;
		constexpr float HeadRadius = 0.22f;
		constexpr int32 RingSegments = 12;

		FVector ReferenceUp(0.0f, 0.0f, 1.0f);
		if (std::abs(Forward.Dot(ReferenceUp)) > 0.98f)
		{
			ReferenceUp = FVector(0.0f, 1.0f, 0.0f);
		}

		const FVector Right = Forward.Cross(ReferenceUp).Normalized();
		const FVector Up = Right.Cross(Forward).Normalized();
		const FVector Tip = Origin + Forward * ArrowLength;
		const FVector HeadBase = Tip - Forward * HeadLength;
		const FColor ShaftColor = FColor::Red();
		const FColor HeadColor = FColor(255, 180, 80);

		Scene.AddDebugLine(Origin, Tip, ShaftColor);

		FVector PreviousRingPoint = HeadBase + Right * HeadRadius;
		for (int32 i = 1; i <= RingSegments; ++i)
		{
			const float Angle = (static_cast<float>(i) / static_cast<float>(RingSegments)) * 2.0f * 3.1415926535f;
			const FVector RingOffset = Right * std::cos(Angle) * HeadRadius + Up * std::sin(Angle) * HeadRadius;
			const FVector RingPoint = HeadBase + RingOffset;

			Scene.AddDebugLine(PreviousRingPoint, RingPoint, HeadColor);
			Scene.AddDebugLine(Tip, RingPoint, HeadColor);
			PreviousRingPoint = RingPoint;
		}

		Scene.AddDebugLine(HeadBase - Right * HeadRadius, HeadBase + Right * HeadRadius, HeadColor);
		Scene.AddDebugLine(HeadBase - Up * HeadRadius, HeadBase + Up * HeadRadius, HeadColor);
	}
}

void UDirectionalLightComponent::ContributeSelectedVisuals(FScene& Scene) const
{
	FVector WorldPos = GetWorldLocation();
	AddDirectionalLightArrow(Scene, WorldPos, GetForwardVector());
}

bool UDirectionalLightComponent::GetLightViewProj(FLightViewProjResult& OutResult, const UCameraComponent* Camera, int32 FaceIndex) const
{
	if (!Camera) return false;

	FGlobalDirectionalLightParams Params;
	Params.Direction = GetForwardVector();

	auto VP = FLightFrustumUtils::BuildDirectionalLightViewProj(
		Params, Camera->GetViewMatrix(), Camera->GetProjectionMatrix());
	OutResult.View = VP.View;
	OutResult.Proj = VP.Proj;
	OutResult.bIsOrtho = true;
	return true;
}

void UDirectionalLightComponent::PushToScene()
{
	if (!Owner) return;
	UWorld* World = Owner->GetWorld();
	if (!World) return;

	FGlobalDirectionalLightParams Params;
	Params.Direction = GetForwardVector();
	Params.Intensity = Intensity;
	Params.LightColor = LightColor;
	Params.bVisible = bVisible;
	Params.bCastShadows = bCastShadows;
	Params.ShadowBias = ShadowBias;
	Params.ShadowSlopeBias = ShadowSlopeBias;
	Params.ShadowNormalBias = ShadowNormalBias;
	Params.ShadowSharpen = ShadowSharpen;
	Params.ShadowResolutionScale = ShadowResolutionScale;

	World->GetScene().GetEnvironment().AddGlobalDirectionalLight(this, Params);
}

void UDirectionalLightComponent::DestroyFromScene()
{
	if (!Owner) return;
	UWorld* World = Owner->GetWorld();
	if (!World) return;

	World->GetScene().GetEnvironment().RemoveGlobalDirectionalLight(this);
}
