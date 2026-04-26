#pragma once
#include "Render/Types/FrameContext.h"
#include "Engine/Collision/Octree.h"

class AActor;
class UWorld;
class FOverlayStatSystem;
class UEditorEngine;
class FScene;
class FOctree;

struct FCollectOutput
{
	TArray<FPrimitiveSceneProxy*> FrustumVisibleProxies;  // GPUOcclusion용
	TArray<FPrimitiveSceneProxy*> RenderableProxies;       // 최종 렌더 대상
	TSet<FPrimitiveSceneProxy*>   VisibleProxySet;         // Decal receiver lookup
};

class FRenderCollector
{
public:
	void Collect(UWorld* World, const FFrameContext& Frame, FCollectOutput& Output);
	void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FScene& Scene);
	void CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FScene& Scene);
	void CollectDebugDraw(const FFrameContext& Frame, FScene& Scene);
	void CollectOctreeDebug(const FOctree* Node, FScene& Scene, uint32 Depth = 0);

private:
	void FilterVisibleProxies(const FFrameContext& Frame, FScene& Scene, FCollectOutput& Output);
	void CollectSelectedActorVisuals(FScene& Scene);
};
