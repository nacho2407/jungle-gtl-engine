#pragma once
#include "Math/Matrix.h"
#include "Slate/SlateUtils.h"
#include "Render/Common/ViewTypes.h"

struct FSceneView
{
	FViewportRect ViewRect;

	FMatrix ViewMatrix;
	FMatrix ProjectionMatrix;
	FMatrix ViewProjectionMatrix;

	FVector CameraPosition;
	FVector CameraForward;
	FVector CameraRight;
	FVector CameraUp;

	EViewMode ViewMode = EViewMode::Lit;

	bool bOrthographic = false;
};

