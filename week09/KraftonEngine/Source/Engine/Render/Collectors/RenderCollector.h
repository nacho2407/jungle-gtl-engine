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


// TODO: 선언 위치 합의 (RenderConstants랑 겹침)

struct FCollectedPrimitives
{
    TArray<FPrimitiveSceneProxy*> VisibleProxies;
    TArray<FPrimitiveSceneProxy*> OpaqueProxies;
    TArray<FPrimitiveSceneProxy*> TransparentProxies;
};

// struct FCollectedLights
// {
//     FGlobalLightConstants GlobalLights;
//     TArray<FLocalLightInfo> LocalLights;
// };

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

    // 마지막 CollectWorld에서 수집된 Primitive 분류 결과
    const FCollectedPrimitives& GetCollectedPrimitives() const { return CollectedPrimitives; }
    const TArray<FPrimitiveSceneProxy*>& GetLastVisibleProxies() const { return CollectedPrimitives.VisibleProxies; }

private:
    void CollectPrimitives(const TArray<FPrimitiveSceneProxy*>& Proxies, const FFrameContext& Frame, FScene& Scene, FRenderer& Renderer);
    void CollectLights(FScene& Scene, FCollectedLights& OutLights);

    FCollectedPrimitives CollectedPrimitives;
    FCollectedLights CollectedLights;
};
