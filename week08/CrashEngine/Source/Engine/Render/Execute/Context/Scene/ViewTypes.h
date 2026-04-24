// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"

/*
    뷰 모드, ShowFlag, 뷰포트 카메라 프리셋, 뷰포트 렌더 옵션을 정의합니다.
    에디터 뷰포트와 렌더 파이프라인이 공통으로 참조합니다.
*/

enum class EViewMode : int32
{
    Lit_Gouraud = 0,
    Unlit       = 1,
    Wireframe   = 2,
    SceneDepth  = 3,
    Lit_Lambert = 4,
    Lit_Phong   = 5,
    WorldNormal = 6,
    Lit         = Lit_Phong,
    Count
};

// FShowFlags는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FShowFlags
{
    bool bPrimitives      = true;
    bool bGrid            = true;
    bool bWorldAxis       = true;
    bool bGizmo           = true;
    bool bBillboardText   = true;
    bool bSceneBVH        = false;
    bool bSceneOctree     = false;
    bool bBoundingVolume  = false;
    bool bWorldBound      = false;
    bool bLightDebugLines = true;
    bool bFog             = true;
    bool bFXAA            = false;
    bool bLightHitMap     = false;
    bool b25DCulling      = true; // false는 tile_based_culling
};

// 뷰포트 카메라 프리셋 (Perspective / 6방향 Orthographic)
enum class ELevelViewportType : uint8
{
    Perspective,
    Top,             // +Z → -Z
    Bottom,          // -Z → +Z
    Left,            // -Y → +Y
    Right,           // +Y → -Y
    Front,           // +X → -X
    Back,            // -X → +X
    FreeOrthographic // 자유 각도 Orthographic
};

// 뷰포트별 렌더 옵션 — 각 뷰포트 클라이언트가 독립적으로 소유
struct FViewportRenderOptions
{
    EViewMode  ViewMode = EViewMode::Lit_Phong;
    FShowFlags ShowFlags;

    float GridSpacing       = 1.0f;
    int32 GridHalfLineCount = 100;

    float              CameraMoveSensitivity   = 1.0f;
    float              CameraRotateSensitivity = 1.0f;
    ELevelViewportType ViewportType            = ELevelViewportType::Perspective;

    // Scene Depth 전용 설정
    int32 SceneDepthVisMode = 1;
    float Exponent          = 1.0f;
    float Range             = 16.0f;

    // FXAA 전용 설정
    float EdgeThreshold    = 0.125f;
    float EdgeThresholdMin = 0.0625f;
};
