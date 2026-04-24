// Defines the per-view camera and visibility state used during rendering.
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

// FSceneView bundles camera state, viewport state, and visibility helpers for one view.
struct FSceneView
{
    FMatrix View;
    FMatrix Proj;
    FVector CameraPosition;
    FVector CameraForward;
    FVector CameraRight;
    FVector CameraUp;
    float   NearClip = 0.1f;
    float   FarClip  = 1000.0f;

    bool  bIsOrtho   = false;
    float OrthoWidth = 10.0f;

    float              ViewportWidth  = 0.0f;
    float              ViewportHeight = 0.0f;
    ELevelViewportType ViewportType   = ELevelViewportType::Perspective;

    FViewportRenderOptions RenderOptions;
    EViewMode              ViewMode = EViewMode::Lit_Phong;
    ERenderShadingPath     RenderPath = ERenderShadingPath::Deferred;
    FShowFlags             ShowFlags;
    FVector                WireframeColor = FVector(0.0f, 0.0f, 0.7f);

    FGPUOcclusionCulling* OcclusionCulling = nullptr;
    FConvexVolume         FrustumVolume;
    FLODUpdateContext     LODContext;

    bool IsFixedOrtho() const
    {
        return bIsOrtho && ViewportType != ELevelViewportType::Perspective && ViewportType != ELevelViewportType::FreeOrthographic;
    }

    void SetCameraInfo(const UCameraComponent* Camera);
    void SetViewportInfo(const FViewport* VP);

    void SetViewportSize(float InWidth, float InHeight)
    {
        ViewportWidth  = InWidth;
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
        RenderOptions.ViewMode  = InViewMode;
        RenderOptions.ShowFlags = InShowFlags;
        ViewMode                = InViewMode;
        ShowFlags               = InShowFlags;
    }
};
