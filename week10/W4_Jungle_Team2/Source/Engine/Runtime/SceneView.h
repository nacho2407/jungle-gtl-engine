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

	// TODO : Editor에서 Engine을 참조하는 의존성 문제로 임시 주석 처리
	// 다른 곳에서 정보를 가지고있어야함. EditorViewportClient면 딱 좋을 것 같은데?
	// EEditorCameraMode CameraMode;
	// EEditorViewMode ViewMode;

	bool bOrthographic = false;
};

