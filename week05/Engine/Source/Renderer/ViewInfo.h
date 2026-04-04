#pragma once

#include "CoreMinimal.h"
#include "Core/ShowFlags.h"

struct ENGINE_API FSceneViewFamily
{
	float Time = 0.0f;
	float DeltaTime = 0.0f;
};

struct ENGINE_API FSceneView
{
	FMatrix ViewMatrix = FMatrix::Identity;
	FMatrix ProjectionMatrix = FMatrix::Identity;
	FShowFlags ShowFlags;
};

struct ENGINE_API FViewInfo : public FSceneView
{
	FMatrix ViewProjectionMatrix = FMatrix::Identity;
	FVector CameraPosition = FVector::ZeroVector;
	float Time = 0.0f;
	float DeltaTime = 0.0f;

	void Initialize(const FSceneViewFamily& InViewFamily, const FSceneView& InView);
	bool HasShowFlag(EEngineShowFlags InFlag) const;
};
