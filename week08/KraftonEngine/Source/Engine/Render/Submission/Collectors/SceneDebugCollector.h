#pragma once

#include "Core/CoreTypes.h"
#include "Engine/Collision/Octree.h"

class FOverlayStatSystem;
class UEditorEngine;
class FScene;
class FPrimitiveSceneProxy;
class FWorldPrimitivePickingBVH;
struct FSceneView; using FFrameContext = FSceneView;

class FSceneDebugCollector
{
public:
    void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FScene& Scene);
    void CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FScene& Scene);
    void CollectDebugDraw(const FFrameContext& Frame, FScene& Scene);
    void CollectOctreeDebug(const FOctree* Node, FScene& Scene, uint32 Depth = 0);
    void CollectWorldBVHDebug(const FWorldPrimitivePickingBVH& BVH, FScene& Scene);
    void CollectWorldBoundsDebug(const TArray<FPrimitiveSceneProxy*>& Proxies, FScene& Scene);
};
