#pragma once

#include "Core/CoreTypes.h"

enum class EViewMode : int32
{
	Lit_Gouraud = 0,
	Unlit = 1,
	Wireframe = 2,
	SceneDepth = 3,
	Lit_Lambert = 4,
	Lit_Phong = 5,
	WorldNormal = 6,
	Lit = Lit_Phong,
	Count
};

struct FShowFlags
{
	bool bPrimitives = true;
	bool bGrid = true;
	bool bWorldAxis = true;
	bool bGizmo = true;
	bool bBillboardText = true;
	bool bDebugDraw = true;
	bool bSceneBVH = false;
	bool bSceneOctree = false;
	bool bBoundingVolume = false;
	bool bWorldBound = false;
	bool bFog = true;
	bool bFXAA = false;
};

// 뷰포트 카메라 프리셋 (Perspective / 6방향 Orthographic)
enum class ELevelViewportType : uint8
{
	Perspective,
	Top,		// +Z → -Z
	Bottom,		// -Z → +Z
	Left,		// -Y → +Y
	Right,		// +Y → -Y
	Front,		// +X → -X
	Back,		// -X → +X
	FreeOrthographic	// 자유 각도 Orthographic
};

// 뷰포트별 렌더 옵션 — 각 뷰포트 클라이언트가 독립적으로 소유
struct FViewportRenderOptions
{
	EViewMode ViewMode = EViewMode::Lit_Phong;
	FShowFlags ShowFlags;

	float GridSpacing = 1.0f;
	int32 GridHalfLineCount = 100;

	float CameraMoveSensitivity = 1.0f;
	float CameraRotateSensitivity = 1.0f;
	ELevelViewportType ViewportType = ELevelViewportType::Perspective;

	// Scene Depth 전용 설정
	int32 SceneDepthVisMode = 1;
	float Exponent = 128.0f;
	float Range = 1000.0f;

	// FXAA 전용 설정
	float EdgeThreshold = 0.125f;
	float EdgeThresholdMin = 0.0625f;
};
