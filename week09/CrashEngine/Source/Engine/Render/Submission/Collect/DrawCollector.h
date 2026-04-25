#pragma once

#include "Render/Submission/Collect/CollectedOverlayData.h"
#include "Render/Submission/Collect/CollectedSceneData.h"

class UWorld;
struct FSceneView;
class FOverlayStatSystem;
class UEditorEngine;
class FScene;
class FOctree;
class FPrimitiveProxy;
class FWorldPrimitivePickingBVH;

// FDrawCollector는 렌더 영역의 핵심 동작을 담당합니다.
class FDrawCollector
{
public:
    void Reset();

    void CollectWorld(UWorld* World, FRenderCollectContext& CollectContext);
    void CollectGrid(float GridSpacing, int32 GridHalfLineCount);
    void CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor);
    void CollectDebugRender(const FScene& Scene);
    void CollectOctreeDebug(const FOctree* Node, uint32 Depth = 0);
    void CollectWorldBVHDebug(const FWorldPrimitivePickingBVH& BVH);
    void CollectWorldBoundsDebug(const TArray<FPrimitiveProxy*>& Proxies);

    void CollectScenePrimitives(UWorld* World, FRenderCollectContext& CollectContext);
    void CollectSceneLights(UWorld* World, FScene* Scene, const FSceneView* SceneView);
    void CollectShadowCasters(UWorld* World, const FSceneView* SceneView);
    void UpdateShadowDataInCBs();
    void CollectOverlay(const FCollectOverlayContext& OverlayContext);

    const FCollectedSceneData&      GetCollectedSceneData() const { return CollectedSceneData; }
    const FCollectedPrimitives&     GetCollectedPrimitives() const { return CollectedSceneData.Primitives; }
    const TArray<FPrimitiveProxy*>& GetLastVisiblePrimitiveProxies() const { return CollectedSceneData.Primitives.VisibleProxies; }
    const FCollectedLights&         GetCollectedLights() const { return CollectedSceneData.Lights; }
    const FCollectedOverlayData&    GetCollectedOverlayData() const { return CollectedOverlayData; }

private:
    // ==================== Reset Helpers ====================
    static void ResetCollectedPrimitives(FCollectedPrimitives& OutPrimitives, bool bClearOverlayTexts);
    static void ResetCollectedLights(FCollectedLights& OutLights);

    // ==================== Scene Helpers ====================
    static uint32 SelectLOD(uint32 CurrentLOD, float DistSq);
    static void   CollectEditorHelpers(UWorld* World, const FSceneView& SceneView, FCollectedOverlayData& OverlayData);

    // ==================== Overlay Helpers ====================
    static void CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FCollectedPrimitives& OutPrimitives);
    static void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FCollectedOverlayData& OverlayData);
    static void CollectDebugRender(const FScene& Scene, FCollectedOverlayData& OverlayData);
    static void CollectOctreeDebug(const FOctree* Node, FCollectedOverlayData& OverlayData, uint32 Depth = 0);
    static void CollectWorldBVHDebug(const FWorldPrimitivePickingBVH& BVH, FCollectedOverlayData& OverlayData);
    static void CollectWorldBoundsDebug(const TArray<FPrimitiveProxy*>& Proxies, FCollectedOverlayData& OverlayData);

private:
    // ==================== Collected Data ====================
    FCollectedSceneData   CollectedSceneData;
    FCollectedOverlayData CollectedOverlayData;
};
