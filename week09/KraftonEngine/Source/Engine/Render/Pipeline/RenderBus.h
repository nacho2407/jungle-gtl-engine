#pragma once
#include "Core/CoreTypes.h"
#include "Render/Pipeline/RenderConstants.h"
#include "Render/Types/ViewTypes.h"

class UCameraComponent;
class FViewport;
class FPrimitiveSceneProxy;


class FRenderBus
{
public:
	void Clear();

	// 프록시 직접 제출 — 포인터만 저장, 데이터는 프록시 소유
	void AddProxy(ERenderPass Pass, const FPrimitiveSceneProxy* Proxy);
	const TArray<const FPrimitiveSceneProxy*>& GetProxies(ERenderPass Pass) const;

	// Batcher 패스용 — 타입 안전한 전용 큐
	void AddFontEntry(FFontEntry&& Entry);
	void AddOverlayFontEntry(FFontEntry&& Entry);
	void AddSubUVEntry(FSubUVEntry&& Entry);
	void AddAABBEntry(FAABBEntry&& Entry);
	void AddGridEntry(FGridEntry&& Entry);

	const TArray<FFontEntry>& GetFontEntries() const { return FontEntries; }
	const TArray<FFontEntry>& GetOverlayFontEntries() const { return OverlayFontEntries; }
	const TArray<FSubUVEntry>& GetSubUVEntries() const { return SubUVEntries; }
	const TArray<FAABBEntry>& GetAABBEntries() const { return AABBEntries; }
	const TArray<FGridEntry>& GetGridEntries() const { return GridEntries; }

	// Getter,Setter
	void SetCameraInfo(const UCameraComponent* Camera);
	void SetViewportInfo(const FViewport* VP);
	void SetViewportSize(float InWidth, float InHeight);
	void SetRenderSettings(const EViewMode NewViewMode, const FShowFlags NewShowFlags);

	const FMatrix& GetView() const { return View; }
	const FMatrix& GetProj() const { return Proj; }
	const FVector& GetCameraForward() const { return CameraForward; }
	const FVector& GetCameraUp() const { return CameraUp; }
	const FVector& GetCameraRight() const { return CameraRight; }
	bool  IsOrtho()        const { return bIsOrtho; }
	bool  IsFixedOrtho()   const { return bIsOrtho && ViewportType != ELevelViewportType::Perspective && ViewportType != ELevelViewportType::FreeOrthographic; }
	float GetOrthoWidth()  const { return OrthoWidth; }
	ELevelViewportType GetViewportType() const { return ViewportType; }
	void SetViewportType(ELevelViewportType InType) { ViewportType = InType; }
	EViewMode GetViewMode() const { return ViewMode; }
	const FShowFlags& GetShowFlags() const { return ShowFlags; }
	const FVector& GetWireframeColor() const { return WireframeColor; }
	void SetWireframeColor(const FVector& InColor) { WireframeColor = InColor; }

	const float GetViewportWidth() const { return viewportWidth; }
	const float GetViewportHeight() const { return viewportHeight; }
	ID3D11RenderTargetView*  GetViewportRTV()        const { return ViewportRTV; }
	ID3D11DepthStencilView*  GetViewportDSV()        const { return ViewportDSV; }
	ID3D11ShaderResourceView* GetViewportStencilSRV() const { return ViewportStencilSRV; }

private:
	// 프록시 패스 큐 — 포인터만 저장, 데이터는 프록시 소유
	TArray<const FPrimitiveSceneProxy*> ProxyQueues[(uint32)ERenderPass::MAX];

	// Batcher 패스 큐
	TArray<FFontEntry>  FontEntries;
	TArray<FFontEntry>  OverlayFontEntries;
	TArray<FSubUVEntry> SubUVEntries;
	TArray<FAABBEntry>  AABBEntries;
	TArray<FGridEntry>  GridEntries;

	FMatrix View;
	FMatrix Proj;
	FVector CameraForward;
	FVector CameraRight;
	FVector CameraUp;

	float viewportWidth = 0.0f;
	float viewportHeight = 0.0f;

	bool  bIsOrtho = false;
	float OrthoWidth = 10.0f;
	ELevelViewportType ViewportType = ELevelViewportType::Perspective;

	// PostProcess용 뷰포트 D3D 리소스 (프레임 내 유효)
	ID3D11RenderTargetView*   ViewportRTV        = nullptr;
	ID3D11DepthStencilView*   ViewportDSV        = nullptr;
	ID3D11ShaderResourceView* ViewportStencilSRV = nullptr;

	//Editor Settings
	EViewMode ViewMode;
	FShowFlags ShowFlags;
	FVector WireframeColor = FVector(0.0f, 0.0f, 0.7f);
};
