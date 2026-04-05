#pragma once
#include "RenderBus.h"
#include "Engine/Collision/Octree.h"

class UWorld;
class FOverlayStatSystem;
class UEditorEngine;
class FScene;
class FDebugDrawQueue;
class FOctree;
class FRenderCollector
{
public:
	void CollectWorld(UWorld* World, FRenderBus& RenderBus);
	void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus);
	void CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FRenderBus& RenderBus);
	void CollectDebugDraw(const FDebugDrawQueue& Queue, FRenderBus& RenderBus);
	void CollectOctreeDebug(const FOctree* Node, FRenderBus& RenderBus, uint32 Depth = 0);

private:
	void CollectVisibleProxies(const TArray<FPrimitiveSceneProxy*>& Proxies, FRenderBus& RenderBus);
};
