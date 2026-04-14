#include "RenderBus.h"
#include "Component/CameraComponent.h"
#include "Viewport/Viewport.h"

void FRenderBus::Clear()
{
	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		ProxyQueues[i].clear();
	}

	FontEntries.clear();
	OverlayFontEntries.clear();
	SubUVEntries.clear();
	BillboardEntries.clear();
	AABBEntries.clear();
	GridEntries.clear();
	DebugLineEntries.clear();

	viewportWidth = 0.0f;
	viewportHeight = 0.0f;
	ViewportRTV = nullptr;
	ViewportDSV = nullptr;
	ViewportStencilSRV = nullptr;
	ViewportDepthSRV = nullptr;
	OcclusionCulling = nullptr;
	LODContext = {};
	ExponentialHeightFog = {};
}

void FRenderBus::AddProxy(ERenderPass Pass, const FPrimitiveSceneProxy* Proxy)
{
	ProxyQueues[(uint32)Pass].push_back(Proxy);
}

const TArray<const FPrimitiveSceneProxy*>& FRenderBus::GetProxies(ERenderPass Pass) const
{
	return ProxyQueues[(uint32)Pass];
}

void FRenderBus::AddFontEntry(FFontEntry&& Entry)
{
	FontEntries.push_back(std::move(Entry));
}

void FRenderBus::AddOverlayFontEntry(FFontEntry&& Entry)
{
	OverlayFontEntries.push_back(std::move(Entry));
}

void FRenderBus::AddSubUVEntry(FSubUVEntry&& Entry)
{
	SubUVEntries.push_back(std::move(Entry));
}

void FRenderBus::AddBillboardEntry(FBillboardEntry&& Entry)
{
	BillboardEntries.push_back(std::move(Entry));
}

void FRenderBus::AddAABBEntry(FAABBEntry&& Entry)
{
	AABBEntries.push_back(std::move(Entry));
}

void FRenderBus::AddGridEntry(FGridEntry&& Entry)
{
	GridEntries.push_back(std::move(Entry));
}

void FRenderBus::AddDebugLineEntry(FDebugLineEntry&& Entry)
{
	DebugLineEntries.push_back(std::move(Entry));
}

void FRenderBus::SetCameraInfo(const UCameraComponent* Camera)
{
	View = Camera->GetViewMatrix();
	Proj = Camera->GetProjectionMatrix();
	CameraPosition = Camera->GetWorldLocation();
	CameraForward = Camera->GetForwardVector();
	CameraRight = Camera->GetRightVector();
	CameraUp = Camera->GetUpVector();
	bIsOrtho = Camera->IsOrthogonal();
	OrthoWidth = Camera->GetOrthoWidth();
	FarClip = Camera->GetFarPlane();
	NearClip = Camera->GetNearPlane();
}

void FRenderBus::SetViewportInfo(const FViewport* VP)
{

	SetRenderTargetInfo(
		static_cast<float>(VP->GetWidth()),
		static_cast<float>(VP->GetHeight()),
		VP->GetRTV(),
		VP->GetDSV(),
		VP->GetDepthSRV(),
		VP->GetStencilSRV(),
		VP->GetNormalRTV(),
		VP->GetNormalSRV(),
		VP->GetAlbedoRTV(), 
		VP->GetAlbedoSRV()
	);
}

void FRenderBus::SetRenderTargetInfo(
	float InWidth,
	float InHeight,
	ID3D11RenderTargetView* InRTV,
	ID3D11DepthStencilView* InDSV,
	ID3D11ShaderResourceView* InDepthSRV,
	ID3D11ShaderResourceView* InStencilSRV,
	ID3D11RenderTargetView* InNormalRTV,
	ID3D11ShaderResourceView* InNormalSRV,
	ID3D11RenderTargetView* InAlbedoRTV,
	ID3D11ShaderResourceView* InAlbedoSRV)
{
	viewportWidth = InWidth;
	viewportHeight = InHeight;
	ViewportRTV = InRTV;
	ViewportDSV = InDSV;
	ViewportDepthSRV = InDepthSRV;
	ViewportStencilSRV = InStencilSRV;
	ViewportNormalRTV = InNormalRTV;
	ViewportNormalSRV = InNormalSRV;
	ViewportAlbedoRTV = InAlbedoRTV;
	ViewportAlbedoSRV = InAlbedoSRV;
}

void FRenderBus::SetRenderSettings(const EViewMode NewViewMode, const FShowFlags NewShowFlags)
{
	ViewMode = NewViewMode;
	ShowFlags = NewShowFlags;
}

void FRenderBus::SetViewportSize(float InWidth, float InHeight)
{
	viewportWidth = InWidth;
	viewportHeight = InHeight;
}
