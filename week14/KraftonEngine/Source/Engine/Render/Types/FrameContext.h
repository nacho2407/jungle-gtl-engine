#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Render/Types/ViewTypes.h"
#include "Render/Types/LODContext.h"
#include "Collision/ConvexVolume.h"

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
	float NearClip = 0.1f;
	float FarClip = 1000.0f;

	bool  bIsOrtho     = false;
	bool  bIsLightView = false;
	float OrthoWidth = 10.0f;

	// Viewport
	float ViewportWidth  = 0.0f;
	float ViewportHeight = 0.0f;

	ID3D11RenderTargetView*   ViewportRTV          = nullptr;
	ID3D11DepthStencilView*   ViewportDSV          = nullptr;
	// SceneColor 복사 — FXAA 등 PostProcess에서 최종 화면 읽기용
	ID3D11ShaderResourceView* SceneColorCopySRV     = nullptr;
	ID3D11Texture2D* SceneColorCopyTexture          = nullptr;
	ID3D11Texture2D* ViewportRenderTexture          = nullptr;

	// CopyResource 소스/대상
	ID3D11Texture2D*          DepthTexture         = nullptr;  // 원본 (CopyResource 소스)
	ID3D11Texture2D*          DepthCopyTexture     = nullptr;  // 복사본 (CopyResource 대상)
	ID3D11ShaderResourceView* DepthCopySRV         = nullptr;
	ID3D11ShaderResourceView* StencilCopySRV       = nullptr;

	// GBuffer Normal RT — Opaque MRT[1] 출력, PostProcess에서 SRV로 읽기
	ID3D11RenderTargetView*   NormalRTV             = nullptr;
	ID3D11ShaderResourceView* NormalSRV             = nullptr;

	// Culling Heatmap RT — Opaque MRT[2] 출력
	ID3D11RenderTargetView*   CullingHeatmapRTV     = nullptr;
	ID3D11ShaderResourceView* CullingHeatmapSRV     = nullptr;

	// Cursor position relative to viewport (for debug visualization)
	uint32 CursorViewportX = UINT32_MAX;
	uint32 CursorViewportY = UINT32_MAX;

	// Render Settings (Single Source of Truth)
	FViewportRenderOptions RenderOptions;

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
			&& RenderOptions.ViewportType != ELevelViewportType::Perspective
			&& RenderOptions.ViewportType != ELevelViewportType::FreeOrthographic;
	}

	// Batch setters - populate multiple fields at once
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

	// Reset D3D pointers
	void ClearViewportResources()
	{
		ViewportRTV             = nullptr;
		ViewportDSV             = nullptr;
		SceneColorCopySRV       = nullptr;
		SceneColorCopyTexture   = nullptr;
		ViewportRenderTexture   = nullptr;
		DepthTexture            = nullptr;
		DepthCopyTexture        = nullptr;
		DepthCopySRV            = nullptr;
		StencilCopySRV          = nullptr;
		NormalRTV               = nullptr;
		NormalSRV               = nullptr;
		CullingHeatmapRTV       = nullptr;
		CullingHeatmapSRV       = nullptr;
	}
};
