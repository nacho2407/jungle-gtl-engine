#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Render/Types/ViewTypes.h"
#include "Render/Execution/LODContext.h"
#include "Render/Visibility/ConvexVolume.h"

class UCameraComponent;
class FViewport;
class FGPUOcclusionCulling;

/*
    FFrameContext - per-frame/per-viewport read-only state.
    Camera, viewport, render settings, occlusion, LOD context.
    Populated once per frame by the render pipeline, then read by
    Renderer, Proxies, and RenderCollector.
*/
struct FFrameContext
{
    // Camera
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

    // Viewport
    float ViewportWidth = 0.0f;
    float ViewportHeight = 0.0f;

    ELevelViewportType ViewportType = ELevelViewportType::Perspective;

    // Render Settings
    FViewportRenderOptions RenderOptions;
    EViewMode ViewMode = EViewMode::Lit_Phong;
    FShowFlags ShowFlags;

    FVector WireframeColor = FVector(0.0f, 0.0f, 0.7f);

    // GPU Occlusion Culling
    FGPUOcclusionCulling* OcclusionCulling = nullptr;

    // Frustum (per-viewport, computed from View * Proj)
    FConvexVolume FrustumVolume;

    // LOD
    FLODUpdateContext LODContext;

    // Derived helpers
    bool IsFixedOrtho() const
    {
        return bIsOrtho && ViewportType != ELevelViewportType::Perspective && ViewportType != ELevelViewportType::FreeOrthographic;
    }

    // Batch setters - populate multiple fields at once
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
    FViewportRenderOptions GetRenderOptions() const { return RenderOptions; }

    void SetRenderSettings(EViewMode InViewMode, const FShowFlags& InShowFlags)
    {
        RenderOptions.ViewMode = InViewMode;
        RenderOptions.ShowFlags = InShowFlags;

        ViewMode = InViewMode;
        ShowFlags = InShowFlags;
    }
};
