#pragma once

#include "Core/CoreTypes.h"
#include "Render/Pipeline/RenderConstants.h"
#include "Render/Types/ViewTypes.h"

class UCameraComponent;
class FViewport;
class FPrimitiveSceneProxy;
class FGPUOcclusionCulling;

#include "Render/Pipeline/LODContext.h"

struct FExponentialHeightFogRenderData
{
	bool bEnabled = false;
	float FogHeight = 0.0f;
	float FogDensity = 0.0f;
	float FogHeightFalloff = 0.0f;
	FVector4 FogColor = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
	float FogMaxOpacity = 1.0f;
	float StartDistance = 0.0f;
	float EndDistance = 0.0f;
	float FogCutoffDistance = 0.0f;
};

class FRenderBus
{
public:
	void Clear();

	void AddProxy(ERenderPass Pass, const FPrimitiveSceneProxy* Proxy);
	const TArray<const FPrimitiveSceneProxy*>& GetProxies(ERenderPass Pass) const;

	void AddFontEntry(FFontEntry&& Entry);
	void AddOverlayFontEntry(FFontEntry&& Entry);
	void AddSubUVEntry(FSubUVEntry&& Entry);
	void AddBillboardEntry(FBillboardEntry&& Entry);
	void AddDecalEntry(FDecalDrawEntry&& Entry);
	void AddAABBEntry(FAABBEntry&& Entry);
	void AddGridEntry(FGridEntry&& Entry);
	void AddDebugLineEntry(FDebugLineEntry&& Entry);

	const TArray<FFontEntry>& GetFontEntries() const { return FontEntries; }
	const TArray<FFontEntry>& GetOverlayFontEntries() const { return OverlayFontEntries; }
	const TArray<FSubUVEntry>& GetSubUVEntries() const { return SubUVEntries; }
	const TArray<FBillboardEntry>& GetBillboardEntries() const { return BillboardEntries; }
	const TArray<FDecalDrawEntry>& GetDecalEntries() const { return DecalEntries; }
	const TArray<FAABBEntry>& GetAABBEntries() const { return AABBEntries; }
	const TArray<FGridEntry>& GetGridEntries() const { return GridEntries; }
	const TArray<FDebugLineEntry>& GetDebugLineEntries() const { return DebugLineEntries; }

	void SetCameraInfo(const UCameraComponent* Camera);
	void SetViewportInfo(const FViewport* VP);
	void SetRenderTargetInfo(
		float InWidth,
		float InHeight,
		ID3D11RenderTargetView* InRTV,
		ID3D11DepthStencilView* InDSV,
		ID3D11ShaderResourceView* InSceneSRV,
		ID3D11ShaderResourceView* InDepthSRV,
		ID3D11ShaderResourceView* InStencilSRV = nullptr,
		ID3D11RenderTargetView* InNormalRTV = nullptr,
		ID3D11ShaderResourceView* InNormalSRV = nullptr,
		ID3D11RenderTargetView* InAlbedoRTV = nullptr,
		ID3D11ShaderResourceView* InAlbedoSRV = nullptr,
		ID3D11RenderTargetView* InPostProcessRTV = nullptr,
		ID3D11ShaderResourceView* InPostProcessSRV = nullptr);
	void SetViewportSize(float InWidth, float InHeight);
	void SetRenderSettings(const EViewMode NewViewMode, const FShowFlags NewShowFlags);
	void SetFXAAEnabled(bool bInEnabled) { bFXAAEnabled = bInEnabled; }
	void SetFXAAConstants(const FFXAAConstants& InConstants) { FXAAConstants = InConstants; }
	void SetExponentialHeightFog(const FExponentialHeightFogRenderData& InFogData) { ExponentialHeightFog = InFogData; }

	const FMatrix& GetView() const { return View; }
	const FMatrix& GetProj() const { return Proj; }
	const FVector& GetCameraPosition() const { return CameraPosition; }
	const FVector& GetCameraForward() const { return CameraForward; }
	const FVector& GetCameraUp() const { return CameraUp; }
	const FVector& GetCameraRight() const { return CameraRight; }
	bool IsOrtho() const { return bIsOrtho; }
	bool IsFixedOrtho() const { return bIsOrtho && ViewportType != ELevelViewportType::Perspective && ViewportType != ELevelViewportType::FreeOrthographic; }
	float GetOrthoWidth() const { return OrthoWidth; }
	ELevelViewportType GetViewportType() const { return ViewportType; }
	void SetViewportType(ELevelViewportType InType) { ViewportType = InType; }
	EViewMode GetViewMode() const { return ViewMode; }
	const FShowFlags& GetShowFlags() const { return ShowFlags; }
	const FVector& GetWireframeColor() const { return WireframeColor; }
	void SetWireframeColor(const FVector& InColor) { WireframeColor = InColor; }
	float GetNearClip() const { return NearClip; }
	float GetFarClip() const { return FarClip; }

	float GetViewportWidth() const { return viewportWidth; }
	float GetViewportHeight() const { return viewportHeight; }
	ID3D11RenderTargetView* GetViewportRTV() const { return ViewportRTV; }
	ID3D11DepthStencilView* GetViewportDSV() const { return ViewportDSV; }
	ID3D11ShaderResourceView* GetViewportSceneSRV() const { return ViewportSceneSRV; }
	ID3D11ShaderResourceView* GetViewportDepthSRV() const { return ViewportDepthSRV; }
	ID3D11ShaderResourceView* GetViewportStencilSRV() const { return ViewportStencilSRV; }
	ID3D11RenderTargetView* GetViewportNormalRTV() const { return ViewportNormalRTV; }
	ID3D11ShaderResourceView* GetViewportNormalSRV() const { return ViewportNormalSRV; }
	ID3D11RenderTargetView* GetViewportAlbedoRTV() const { return ViewportAlbedoRTV; }
	ID3D11ShaderResourceView* GetViewportAlbedoSRV() const { return ViewportAlbedoSRV; }
	ID3D11RenderTargetView* GetViewportPostProcessRTV() const { return ViewportPostProcessRTV; }
	ID3D11ShaderResourceView* GetViewportPostProcessSRV() const { return ViewportPostProcessSRV; }
	bool IsFXAAEnabled() const { return bFXAAEnabled; }
	const FFXAAConstants& GetFXAAConstants() const { return FXAAConstants; }
	const FExponentialHeightFogRenderData& GetExponentialHeightFog() const { return ExponentialHeightFog; }

	void SetOcclusionCulling(FGPUOcclusionCulling* InOcclusion) { OcclusionCulling = InOcclusion; }
	const FGPUOcclusionCulling* GetOcclusionCulling() const { return OcclusionCulling; }
	FGPUOcclusionCulling* GetOcclusionCullingMutable() const { return OcclusionCulling; }

	void SetLODContext(const FLODUpdateContext& InCtx) { LODContext = InCtx; }
	const FLODUpdateContext& GetLODContext() const { return LODContext; }

private:
	TArray<const FPrimitiveSceneProxy*> ProxyQueues[(uint32)ERenderPass::MAX];

	TArray<FFontEntry> FontEntries;
	TArray<FFontEntry> OverlayFontEntries;
	TArray<FSubUVEntry> SubUVEntries;
	TArray<FBillboardEntry> BillboardEntries;
	TArray<FDecalDrawEntry> DecalEntries;
	TArray<FAABBEntry> AABBEntries;
	TArray<FGridEntry> GridEntries;
	TArray<FDebugLineEntry> DebugLineEntries;

	FMatrix View;
	FMatrix Proj;
	FVector CameraPosition;
	FVector CameraForward;
	FVector CameraRight;
	FVector CameraUp;
	float NearClip = 0.1f;
	float FarClip = 1000.0f;

	float viewportWidth = 0.0f;
	float viewportHeight = 0.0f;

	bool bIsOrtho = false;
	float OrthoWidth = 10.0f;
	ELevelViewportType ViewportType = ELevelViewportType::Perspective;

	ID3D11RenderTargetView* ViewportRTV = nullptr;
	ID3D11DepthStencilView* ViewportDSV = nullptr;
	ID3D11ShaderResourceView* ViewportSceneSRV = nullptr;
	ID3D11ShaderResourceView* ViewportDepthSRV = nullptr;
	ID3D11ShaderResourceView* ViewportStencilSRV = nullptr;
	ID3D11RenderTargetView* ViewportNormalRTV = nullptr;
	ID3D11ShaderResourceView* ViewportNormalSRV = nullptr;
	ID3D11RenderTargetView* ViewportAlbedoRTV = nullptr;
	ID3D11ShaderResourceView* ViewportAlbedoSRV = nullptr;
	ID3D11RenderTargetView* ViewportPostProcessRTV = nullptr;
	ID3D11ShaderResourceView* ViewportPostProcessSRV = nullptr;

	FGPUOcclusionCulling* OcclusionCulling = nullptr;
	FLODUpdateContext LODContext;

	bool bFXAAEnabled = false;
	FFXAAConstants FXAAConstants;
	FExponentialHeightFogRenderData ExponentialHeightFog;

	EViewMode ViewMode;
	FShowFlags ShowFlags;
	FVector WireframeColor = FVector(0.0f, 0.0f, 0.7f);
};
