#pragma once

#include "Render/Pipelines/Context/View/SceneView.h"
#include "Render/Submission/Collectors/SceneVisibilityCollector.h"
#include "Render/Submission/Collectors/SceneDebugCollector.h"

class UWorld;
class FOverlayStatSystem;
class UEditorEngine;
class FScene;
class FOctree;
class FRenderer;
class FPrimitiveSceneProxy;

class FRenderCollector
{
public:
    void CollectWorld(UWorld* World, const FSceneView& SceneView, FScene& Scene, FRenderer& Renderer);
    void BuildFramePassCommands(const FSceneView& SceneView, FScene& Scene, FRenderer& Renderer);

    void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FScene& Scene) { DebugCollector.CollectGrid(GridSpacing, GridHalfLineCount, Scene); }
    void CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FScene& Scene) { DebugCollector.CollectOverlayText(OverlaySystem, Editor, Scene); }
    void CollectDebugDraw(const FSceneView& SceneView, FScene& Scene) { DebugCollector.CollectDebugDraw(SceneView, Scene); }
    void CollectOctreeDebug(const FOctree* Node, FScene& Scene, uint32 Depth = 0) { DebugCollector.CollectOctreeDebug(Node, Scene, Depth); }
    void CollectWorldBVHDebug(const class FWorldPrimitivePickingBVH& BVH, FScene& Scene) { DebugCollector.CollectWorldBVHDebug(BVH, Scene); }
    void CollectWorldBoundsDebug(const TArray<FPrimitiveSceneProxy*>& Proxies, FScene& Scene) { DebugCollector.CollectWorldBoundsDebug(Proxies, Scene); }

    const FCollectedPrimitives& GetCollectedPrimitives() const { return VisibilityCollector.GetCollectedPrimitives(); }
    const TArray<FPrimitiveSceneProxy*>& GetLastVisibleProxies() const { return VisibilityCollector.GetLastVisibleProxies(); }
    const FCollectedLights& GetCollectedLights() const { return VisibilityCollector.GetCollectedLights(); }

private:
    FSceneVisibilityCollector VisibilityCollector;
    FSceneDebugCollector DebugCollector;
};
