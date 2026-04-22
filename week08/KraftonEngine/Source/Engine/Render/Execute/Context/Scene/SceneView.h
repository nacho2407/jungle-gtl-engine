#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Render/Execute/Context/Scene/ViewTypes.h"
#include "Render/Visibility/LOD/LODContext.h"
#include "Render/Visibility/Frustum/ConvexVolume.h"

class UCameraComponent;
class FViewport;
class FGPUOcclusionCulling;
class FTileBasedLightCulling;
    /*
    �� ���� ����Ʈ/ī�޶� �������� �������� �� �ʿ��� �б� ���� �� �����Դϴ�.
    ī�޶� ���, �� ���, ShowFlag, ��������, LOD ������ �Բ� �����մϴ�.
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

