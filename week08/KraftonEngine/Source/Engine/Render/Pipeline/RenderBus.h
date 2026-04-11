#pragma once
#include "Core/CoreTypes.h"
#include "Render/Pipeline/RenderConstants.h"
#include "Render/Pipeline/FrameContext.h"

/*
	FRenderBus - per-frame data transport.
	Owns FFrameContext (read-only state) + lightweight data arrays.
	Populated by RenderPipeline + RenderCollector, consumed by Renderer.
	ProxyQueues 제거됨 — Collector가 Renderer.BuildCommandForProxy()를 직접 호출.
*/
class FRenderBus
{
public:
	void Clear();

	// ===== Frame Context (camera, viewport, settings) =====
	FFrameContext Frame;

	// ===== Overlay text (screen-space) =====
	struct FOverlayText { FString Text; FVector2 Position; float Scale; };
	void AddOverlayText(FString Text, const FVector2& Position, float Scale);
	const TArray<FOverlayText>& GetOverlayTexts() const { return OverlayTexts; }

	// ===== Debug AABB =====
	struct FDebugAABB { FVector Min; FVector Max; FColor Color; };
	void AddDebugAABB(const FVector& Min, const FVector& Max, const FColor& Color);
	const TArray<FDebugAABB>& GetDebugAABBs() const { return DebugAABBs; }

	// ===== Debug lines =====
	struct FDebugLine { FVector Start; FVector End; FColor Color; };
	void AddDebugLine(const FVector& Start, const FVector& End, const FColor& Color);
	const TArray<FDebugLine>& GetDebugLines() const { return DebugLines; }

	// ===== Grid =====
	void SetGrid(float Spacing, int32 HalfLineCount);
	bool HasGrid() const { return bHasGrid; }
	float GetGridSpacing() const { return GridSpacing; }
	int32 GetGridHalfLineCount() const { return GridHalfLineCount; }

private:
	TArray<FOverlayText> OverlayTexts;
	TArray<FDebugAABB>   DebugAABBs;
	TArray<FDebugLine>   DebugLines;

	float GridSpacing = 0.0f;
	int32 GridHalfLineCount = 0;
	bool  bHasGrid = false;
};
