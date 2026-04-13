#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Render/Types/ViewTypes.h"
#include "Render/Pipeline/LODContext.h"
#include "Render/Culling/ConvexVolume.h"

#include <d3d11.h>

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

	bool  bIsOrtho   = false;
	float OrthoWidth = 10.0f;

	// Viewport
	float ViewportWidth  = 0.0f;
	float ViewportHeight = 0.0f;

	ID3D11RenderTargetView*   ViewportRTV          = nullptr;
	ID3D11DepthStencilView*   ViewportDSV          = nullptr;
	ID3D11DepthStencilView*   ViewportDSVReadOnly  = nullptr;  // SRV와 동시 바인딩 가능
	ID3D11ShaderResourceView* ViewportStencilSRV   = nullptr;
	ID3D11ShaderResourceView* ViewportDepthSRV     = nullptr;

	ELevelViewportType ViewportType = ELevelViewportType::Perspective;

	// Render Settings
	EViewMode  ViewMode = EViewMode::Lit;
	FShowFlags ShowFlags;
	FVector    WireframeColor = FVector(0.0f, 0.0f, 0.7f);

	// GPU Occlusion Culling
	FGPUOcclusionCulling* OcclusionCulling = nullptr;

	// Frustum (per-viewport, computed from View * Proj)
	FConvexVolume FrustumVolume;

	// LOD
	FLODUpdateContext LODContext;

	// Derived helpers
	bool IsFixedOrtho() const
	{
		return bIsOrtho
			&& ViewportType != ELevelViewportType::Perspective
			&& ViewportType != ELevelViewportType::FreeOrthographic;
	}

	// Batch setters - populate multiple fields at once
	void SetCameraInfo(const UCameraComponent* Camera);
	void SetViewportInfo(const FViewport* VP);

	void SetViewportSize(float InWidth, float InHeight)
	{
		ViewportWidth  = InWidth;
		ViewportHeight = InHeight;
	}

	void SetRenderSettings(EViewMode InViewMode, const FShowFlags& InShowFlags)
	{
		ViewMode  = InViewMode;
		ShowFlags = InShowFlags;
	}

	// Reset D3D pointers
	void ClearViewportResources()
	{
		ViewportRTV         = nullptr;
		ViewportDSV         = nullptr;
		ViewportDSVReadOnly = nullptr;
		ViewportStencilSRV  = nullptr;
		ViewportDepthSRV    = nullptr;
	}
};
