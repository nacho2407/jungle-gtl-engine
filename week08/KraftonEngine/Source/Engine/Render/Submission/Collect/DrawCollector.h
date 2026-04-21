#pragma once

#include "Render/Submission/Collect/CollectedOverlayData.h"
#include "Render/Submission/Collect/CollectedSceneData.h"

class UWorld;
struct FSceneView;
class FOverlayStatSystem;
class UEditorEngine;
class FScene;
class FOctree;
class FPrimitiveSceneProxy;
class FWorldPrimitivePickingBVH;

/*
    이번 프레임에 그릴 대상을 수집하는 클래스입니다.
    Primitive/Light/Overlay를 FDrawCollector가 직접 수집하고,
    OverlayText는 Primitive 수집 결과에 포함합니다.
*/
class FDrawCollector
{
public:
    void Reset();

    void CollectWorld(UWorld* World, FRenderCollectContext& CollectContext);
    void CollectGrid(float GridSpacing, int32 GridHalfLineCount);
    void CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor);
    void CollectDebugDraw(const FSceneView& SceneView, const FScene& Scene);
    void CollectOctreeDebug(const FOctree* Node, uint32 Depth = 0);
    void CollectWorldBVHDebug(const FWorldPrimitivePickingBVH& BVH);
    void CollectWorldBoundsDebug(const TArray<FPrimitiveSceneProxy*>& Proxies);

    void CollectScenePrimitives(UWorld* World, FRenderCollectContext& CollectContext);
    void CollectSceneLights(FScene* Scene);
    void CollectOverlay(const FCollectOverlayContext& OverlayContext);

    const FCollectedSceneData&           GetCollectedSceneData() const { return CollectedSceneData; }
    const FCollectedPrimitives&          GetCollectedPrimitives() const { return CollectedSceneData.Primitives; }
    const TArray<FPrimitiveSceneProxy*>& GetLastVisiblePrimitiveProxies() const { return CollectedSceneData.Primitives.VisibleProxies; }
    const FCollectedLights&              GetCollectedLights() const { return CollectedSceneData.Lights; }
    const FCollectedOverlayData&         GetCollectedOverlayData() const { return CollectedOverlayData; }

private:
    // ==================== Reset Helpers ====================
    static void ResetCollectedPrimitives(FCollectedPrimitives& OutPrimitives, bool bClearOverlayTexts);
    static void ResetCollectedLights(FCollectedLights& OutLights);

    // ==================== Scene Helpers ====================
    static uint32 SelectLOD(uint32 CurrentLOD, float DistSq);

    // ==================== Overlay Helpers ====================
    static void CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FCollectedPrimitives& OutPrimitives);
    static void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FCollectedOverlayData& OverlayData);
    static void CollectDebugDraw(const FSceneView& SceneView, const FScene& Scene, FCollectedOverlayData& OverlayData);
    static void CollectOctreeDebug(const FOctree* Node, FCollectedOverlayData& OverlayData, uint32 Depth = 0);
    static void CollectWorldBVHDebug(const FWorldPrimitivePickingBVH& BVH, FCollectedOverlayData& OverlayData);
    static void CollectWorldBoundsDebug(const TArray<FPrimitiveSceneProxy*>& Proxies, FCollectedOverlayData& OverlayData);

private:
    // ==================== Collected Data ====================
    FCollectedSceneData   CollectedSceneData;
    FCollectedOverlayData CollectedOverlayData;
};
