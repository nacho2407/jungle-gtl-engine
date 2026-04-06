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
	AABBEntries.clear();
	GridEntries.clear();
	DebugLineEntries.clear();

	ViewportRTV = nullptr;
	ViewportDSV = nullptr;
	ViewportStencilSRV = nullptr;
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
}

void FRenderBus::SetViewportInfo(const FViewport* VP)
{
	viewportWidth = static_cast<float>(VP->GetWidth());
	viewportHeight = static_cast<float>(VP->GetHeight());
	ViewportRTV = VP->GetRTV();
	ViewportDSV = VP->GetDSV();
	ViewportStencilSRV = VP->GetStencilSRV();
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
