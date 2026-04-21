#include "Render/Submission/Collect/DrawCollector.h"

#include "Collision/WorldPrimitivePickingBVH.h"
#include "Editor/EditorEngine.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Engine/Collision/Octree.h"
#include "Render/Pipelines/Context/Scene/SceneView.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Scene/Scene.h"

// ==================== Public API ====================

void FDrawCollector::CollectOverlay(const FCollectOverlayContext& OverlayContext)
{
    CollectedSceneData.Primitives.OverlayTexts.clear();
    CollectedOverlayData.ClearTransientData();

    if (OverlayContext.OverlaySystem && OverlayContext.Editor)
    {
        CollectOverlayText(*OverlayContext.OverlaySystem, *OverlayContext.Editor, CollectedSceneData.Primitives);
    }

    CollectGrid(OverlayContext.GridSpacing, OverlayContext.GridHalfLineCount, CollectedOverlayData);

    if (OverlayContext.SceneView && OverlayContext.Scene)
    {
        CollectDebugDraw(*OverlayContext.SceneView, *OverlayContext.Scene, CollectedOverlayData);
    }

    if (OverlayContext.Octree)
    {
        CollectOctreeDebug(OverlayContext.Octree, CollectedOverlayData);
    }

    if (OverlayContext.WorldBVH)
    {
        CollectWorldBVHDebug(*OverlayContext.WorldBVH, CollectedOverlayData);
    }

    if (OverlayContext.WorldBoundsProxies)
    {
        CollectWorldBoundsDebug(*OverlayContext.WorldBoundsProxies, CollectedOverlayData);
    }
}

void FDrawCollector::CollectGrid(float GridSpacing, int32 GridHalfLineCount)
{
    CollectGrid(GridSpacing, GridHalfLineCount, CollectedOverlayData);
}

void FDrawCollector::CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor)
{
    CollectOverlayText(OverlaySystem, Editor, CollectedSceneData.Primitives);
}

void FDrawCollector::CollectDebugDraw(const FSceneView& SceneView, const FScene& Scene)
{
    CollectDebugDraw(SceneView, Scene, CollectedOverlayData);
}

void FDrawCollector::CollectOctreeDebug(const FOctree* Node, uint32 Depth)
{
    CollectOctreeDebug(Node, CollectedOverlayData, Depth);
}

void FDrawCollector::CollectWorldBVHDebug(const FWorldPrimitivePickingBVH& BVH)
{
    CollectWorldBVHDebug(BVH, CollectedOverlayData);
}

void FDrawCollector::CollectWorldBoundsDebug(const TArray<FPrimitiveSceneProxy*>& Proxies)
{
    CollectWorldBoundsDebug(Proxies, CollectedOverlayData);
}


// ==================== Overlay Helpers ====================

void FDrawCollector::CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FCollectedPrimitives& OutPrimitives)
{
    TArray<FOverlayStatLine> Lines;
    OverlaySystem.BuildLines(Editor, Lines);

    const float Scale = OverlaySystem.GetLayout().TextScale;
    for (const FOverlayStatLine& Line : Lines)
    {
        if (!Line.Text.empty())
        {
            OutPrimitives.OverlayTexts.push_back({ Line.Text, Line.ScreenPosition, Scale });
        }
    }
}

void FDrawCollector::CollectGrid(float GridSpacing, int32 GridHalfLineCount, FCollectedOverlayData& OverlayData)
{
    OverlayData.Guides.Grid.Spacing       = GridSpacing;
    OverlayData.Guides.Grid.HalfLineCount = GridHalfLineCount;
    OverlayData.Guides.Grid.bEnabled      = (GridSpacing > 0.0f && GridHalfLineCount > 0);
}

void FDrawCollector::CollectDebugDraw(const FSceneView& SceneView, const FScene& Scene, FCollectedOverlayData& OverlayData)
{
    OverlayData.Debug.Queue = Scene.GetDebugDrawQueue();

    for (const FDebugDrawItem& Item : Scene.GetImmediateDebugLines())
    {
        OverlayData.Debug.Lines.push_back({ Item.Start, Item.End, Item.Color });
    }

    if (!SceneView.ShowFlags.bDebugDraw)
    {
        return;
    }

    for (const FDebugDrawItem& Item : Scene.GetDebugDrawQueue().GetItems())
    {
        OverlayData.Debug.Lines.push_back({ Item.Start, Item.End, Item.Color });
    }
}

void FDrawCollector::CollectOctreeDebug(const FOctree* Node, FCollectedOverlayData& OverlayData, uint32 Depth)
{
    (void)Depth;
    if (!Node)
    {
        return;
    }

    const FVector Min = Node->GetCellBounds().Min;
    const FVector Max = Node->GetCellBounds().Max;
    OverlayData.Debug.AABBs.push_back({ Min, Max, FColor(0, 255, 255) });

    const FVector Corners[8] = {
        { Min.X, Min.Y, Min.Z },
        { Max.X, Min.Y, Min.Z },
        { Max.X, Max.Y, Min.Z },
        { Min.X, Max.Y, Min.Z },
        { Min.X, Min.Y, Max.Z },
        { Max.X, Min.Y, Max.Z },
        { Max.X, Max.Y, Max.Z },
        { Min.X, Max.Y, Max.Z },
    };

    static const int32 EdgePairs[12][2] = {
        { 0, 1 },
        { 1, 2 },
        { 2, 3 },
        { 3, 0 },
        { 4, 5 },
        { 5, 6 },
        { 6, 7 },
        { 7, 4 },
        { 0, 4 },
        { 1, 5 },
        { 2, 6 },
        { 3, 7 },
    };

    for (const auto& Edge : EdgePairs)
    {
        OverlayData.Debug.Lines.push_back({ Corners[Edge[0]], Corners[Edge[1]], FColor(0, 255, 255) });
    }

    for (const FOctree* Child : Node->GetChildren())
    {
        CollectOctreeDebug(Child, OverlayData, Depth + 1);
    }
}

void FDrawCollector::CollectWorldBVHDebug(const FWorldPrimitivePickingBVH& BVH, FCollectedOverlayData& OverlayData)
{
    const TArray<FWorldPrimitivePickingBVH::FNode>& Nodes = BVH.GetNodes();
    for (const FWorldPrimitivePickingBVH::FNode& Node : Nodes)
    {
        if (!Node.Bounds.IsValid())
        {
            continue;
        }

        OverlayData.Debug.AABBs.push_back({ Node.Bounds.Min, Node.Bounds.Max, FColor(0, 255, 0) });
    }
}

void FDrawCollector::CollectWorldBoundsDebug(const TArray<FPrimitiveSceneProxy*>& Proxies, FCollectedOverlayData& OverlayData)
{
    for (FPrimitiveSceneProxy* Proxy : Proxies)
    {
        if (!Proxy || !Proxy->CachedBounds.IsValid())
        {
            continue;
        }

        OverlayData.Debug.AABBs.push_back({ Proxy->CachedBounds.Min, Proxy->CachedBounds.Max, FColor(255, 0, 255) });
    }
}
