#pragma once
#include "Math/Matrix.h"
#include "Render/Common/ViewTypes.h"
#include "ViewportRect.h"

/**
 * 포인터 등 런타임 도중 상태가 변할 수 있는 mutable 데이터가 아닌 immutable 데이터만 보관
 * 렌더링에 필요한 프레임 스냅샷 데이터라고 볼 수 있음
 */
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

	float CameraOrthoHeight;

	FFrustum CameraFrustum;

	EViewMode ViewMode = EViewMode::Lit;

	bool bOrthographic = false;
};

