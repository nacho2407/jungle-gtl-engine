#pragma once
#include "RenderBus.h"
#include "Engine/Collision/Octree.h"

class UWorld;
class FOverlayStatSystem;
class UEditorEngine;
class FScene;

class FRenderCollector {
public:
	void CollectWorld(UWorld* World, FRenderBus& RenderBus);
	void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus);
	void CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FRenderBus& RenderBus);

private:
	void CollectFromScene(FScene& Scene, FRenderBus& RenderBus);
	void CollectOctree(const FOctree* Octree, FRenderBus& RenderBus);
	void CollectOctreeNode(const FOctree& Node, uint32 Depth, FRenderBus& RenderBus);
};
