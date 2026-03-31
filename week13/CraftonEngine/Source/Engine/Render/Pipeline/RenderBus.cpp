#include "RenderBus.h"

void FRenderBus::Clear()
{
	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		PassQueues[i].clear();
	}

	FontEntries.clear();
	OverlayFontEntries.clear();
	SubUVEntries.clear();
	AABBEntries.clear();
	GridEntries.clear();
}

void FRenderBus::AddCommand(ERenderPass Pass, const FRenderCommand& InCommand)
{
	PassQueues[(uint32)Pass].push_back(InCommand);
}

void FRenderBus::AddCommand(ERenderPass Pass, FRenderCommand&& InCommand)
{
	PassQueues[(uint32)Pass].push_back(std::move(InCommand));
}

const TArray<FRenderCommand>& FRenderBus::GetCommands(ERenderPass Pass) const
{
	return PassQueues[(uint32)Pass];
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

void FRenderBus::SetViewProjection(const FMatrix& InView, const FMatrix& InProj, const FVector& CameraForwardVector, const FVector& CameraRightVector, const FVector& CameraUpVector)
{
	View = InView;
	Proj = InProj;
	CameraForward = CameraForwardVector;
	CameraRight = CameraRightVector;
	CameraUp = CameraUpVector;
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
