#pragma once
#include "Math/Matrix.h"
#include "Slate/SlateUtils.h"

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


	// EEditorCameraMode CameraMode;
	// EEditorViewMode ViewMode;

	bool bOrthographic = false;
};

