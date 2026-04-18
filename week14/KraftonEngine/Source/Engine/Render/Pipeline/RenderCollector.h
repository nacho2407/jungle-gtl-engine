#pragma once
#include "Render/Pipeline/FrameContext.h"
#include "Engine/Collision/Octree.h"

class UWorld;
class FOverlayStatSystem;
class UEditorEngine;
class FScene;
class FOctree;
class FDrawCommandBuilder;

class FRenderCollector
{
public:
	void CollectWorld(UWorld* World, const FFrameContext& Frame, FDrawCommandBuilder& Builder);
	void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FScene& Scene);
	void CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FScene& Scene);
	void CollectDebugDraw(const FFrameContext& Frame, FScene& Scene);
	void CollectOctreeDebug(const FOctree* Node, FScene& Scene, uint32 Depth = 0);

	// 마지막 CollectWorld에서 수집된 visible 프록시 (Occlusion Test용)
	const TArray<FPrimitiveSceneProxy*>& GetLastVisibleProxies() const { return LastVisibleProxies; }

private:
	void CollectVisibleProxies(const TArray<FPrimitiveSceneProxy*>& Proxies, const FFrameContext& Frame, FScene& Scene, FDrawCommandBuilder& Builder);

	// CollectVisibleProxies 서브 메서드
	void CollectFontProxy(const FPrimitiveSceneProxy* Proxy, const FFrameContext& Frame, FDrawCommandBuilder& Builder);
	void CollectDecalProxy(FPrimitiveSceneProxy* Proxy, const FFrameContext& Frame,
		const TSet<FPrimitiveSceneProxy*>& VisibleSet, FDrawCommandBuilder& Builder);
	void CollectMeshProxy(const FPrimitiveSceneProxy* Proxy, FDrawCommandBuilder& Builder);
	void CollectSelectionVisuals(FPrimitiveSceneProxy* Proxy, bool bShowBoundingVolume,
		FScene& Scene, FDrawCommandBuilder& Builder);

	TArray<FPrimitiveSceneProxy*> LastVisibleProxies;
};
