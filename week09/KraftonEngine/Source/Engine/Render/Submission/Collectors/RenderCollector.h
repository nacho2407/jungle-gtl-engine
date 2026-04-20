#pragma once

#include "Render/Frame/FrameContext.h"
#include "Render/Core/RenderConstants.h"
#include "Engine/Collision/Octree.h"

class UWorld;
class FOverlayStatSystem;
class UEditorEngine;
class FScene;
class FOctree;
class FRenderer;

class FPrimitiveSceneProxy;
class FLightSceneProxy;

struct FCollectedPrimitives
{
    TArray<FPrimitiveSceneProxy*> VisibleProxies;
    TArray<FPrimitiveSceneProxy*> OpaqueProxies;
    TArray<FPrimitiveSceneProxy*> TransparentProxies;
};

class FRenderCollector
{
public:
    void CollectWorld(UWorld* World, const FFrameContext& Frame, FScene& Scene, FRenderer& Renderer);
    void BuildFramePassCommands(const FFrameContext& Frame, FScene& Scene, FRenderer& Renderer);
    void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FScene& Scene);
    void CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FScene& Scene);
    void CollectDebugDraw(const FFrameContext& Frame, FScene& Scene);
    void CollectOctreeDebug(const FOctree* Node, FScene& Scene, uint32 Depth = 0);
    void CollectWorldBVHDebug(const class FWorldPrimitivePickingBVH& BVH, FScene& Scene);
    void CollectWorldBoundsDebug(const TArray<FPrimitiveSceneProxy*>& Proxies, FScene& Scene);

    const FCollectedPrimitives& GetCollectedPrimitives() const { return CollectedPrimitives; }
    const TArray<FPrimitiveSceneProxy*>& GetLastVisibleProxies() const { return CollectedPrimitives.VisibleProxies; }
    const FCollectedLights& GetCollectedLights() const { return CollectedLights; }

private:
    void CollectPrimitives(const TArray<FPrimitiveSceneProxy*>& Proxies, const FFrameContext& Frame, FScene& Scene, FRenderer& Renderer);
    void CollectLights(FScene& Scene, FCollectedLights& OutLights);

    FCollectedPrimitives CollectedPrimitives;
    FCollectedLights CollectedLights;
};
