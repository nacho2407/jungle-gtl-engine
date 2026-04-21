#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Render/Pipelines/Context/Scene/ViewTypes.h"
#include "Render/Visibility/LOD/LODContext.h"
#include "Render/Visibility/Frustum/ConvexVolume.h"

class UCameraComponent;
class FViewport;
class FGPUOcclusionCulling;
class FTileBasedLightCulling;
    /*
    한 개의 뷰포트/카메라 시점에서 렌더링할 때 필요한 읽기 전용 뷰 정보입니다.
    카메라 행렬, 뷰 모드, ShowFlag, 프러스텀, LOD 문맥을 함께 보관합니다.
*/
struct FSceneView
{
    FMatrix View;
    FMatrix Proj;
    FVector CameraPosition;
    FVector CameraForward;
    FVector CameraRight;
    FVector CameraUp;
    float NearClip = 0.1f;
    float FarClip = 1000.0f;

    bool bIsOrtho = false;
    float OrthoWidth = 10.0f;

    float ViewportWidth = 0.0f;
    float ViewportHeight = 0.0f;
    ELevelViewportType ViewportType = ELevelViewportType::Perspective;

    FViewportRenderOptions RenderOptions;
    EViewMode ViewMode = EViewMode::Lit_Phong;
    FShowFlags ShowFlags;
    FVector WireframeColor = FVector(0.0f, 0.0f, 0.7f);

    FGPUOcclusionCulling* OcclusionCulling = nullptr;
    FTileBasedLightCulling* LightCulling = nullptr;
    FConvexVolume FrustumVolume;
    FLODUpdateContext LODContext;

    bool IsFixedOrtho() const
    {
        return bIsOrtho && ViewportType != ELevelViewportType::Perspective && ViewportType != ELevelViewportType::FreeOrthographic;
    }

    void SetCameraInfo(const UCameraComponent* Camera);
    void SetViewportInfo(const FViewport* VP);

    void SetViewportSize(float InWidth, float InHeight)
    {
        ViewportWidth = InWidth;
        ViewportHeight = InHeight;
    }

    void SetRenderOptions(const FViewportRenderOptions& InOptions)
    {
        RenderOptions = InOptions;
    }

    FViewportRenderOptions GetRenderOptions() const
    {
        return RenderOptions;
    }

    void SetRenderSettings(EViewMode InViewMode, const FShowFlags& InShowFlags)
    {
        RenderOptions.ViewMode = InViewMode;
        RenderOptions.ShowFlags = InShowFlags;
        ViewMode = InViewMode;
        ShowFlags = InShowFlags;
    }
};

