#include "RenderBus.h"

void FRenderBus::Clear()
{
	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
		ProxyQueues[i].clear();

	OverlayTexts.clear();
	DebugAABBs.clear();
	DebugLines.clear();
	bHasGrid = false;

	Frame.ClearViewportResources();
}

void FRenderBus::AddProxy(ERenderPass Pass, const FPrimitiveSceneProxy* Proxy)
{
	ProxyQueues[(uint32)Pass].push_back(Proxy);
}

const TArray<const FPrimitiveSceneProxy*>& FRenderBus::GetProxies(ERenderPass Pass) const
{
	return ProxyQueues[(uint32)Pass];
}

void FRenderBus::AddOverlayText(FString Text, const FVector2& Position, float Scale)
{
	OverlayTexts.push_back({ std::move(Text), Position, Scale });
}

void FRenderBus::AddDebugAABB(const FVector& Min, const FVector& Max, const FColor& Color)
{
	DebugAABBs.push_back({ Min, Max, Color });
}

void FRenderBus::AddDebugLine(const FVector& Start, const FVector& End, const FColor& Color)
{
	DebugLines.push_back({ Start, End, Color });
}

void FRenderBus::SetGrid(float Spacing, int32 HalfLineCount)
{
	GridSpacing = Spacing;
	GridHalfLineCount = HalfLineCount;
	bHasGrid = true;
}
