#pragma once

#include "Render/Pipelines/Context/View/SceneView.h"
#include "Render/Resources/RenderResources.h"

class UWorld;
class FScene;
class FRenderer;
class FPrimitiveSceneProxy;

struct FCollectedPrimitives
{
    TArray<FPrimitiveSceneProxy*> VisibleProxies;
    TArray<FPrimitiveSceneProxy*> OpaqueProxies;
    TArray<FPrimitiveSceneProxy*> TransparentProxies;
};

class FSceneVisibilityCollector
{
public:
    void CollectWorld(UWorld* World, const FFrameContext& Frame, FScene& Scene, FRenderer& Renderer);

    const FCollectedPrimitives& GetCollectedPrimitives() const { return CollectedPrimitives; }
    const TArray<FPrimitiveSceneProxy*>& GetLastVisibleProxies() const { return CollectedPrimitives.VisibleProxies; }
    const FCollectedLights& GetCollectedLights() const { return CollectedLights; }

private:
    void CollectPrimitives(const TArray<FPrimitiveSceneProxy*>& Proxies, const FFrameContext& Frame, FScene& Scene, FRenderer& Renderer);
    void CollectLights(FScene& Scene, FCollectedLights& OutLights);

    FCollectedPrimitives CollectedPrimitives;
    FCollectedLights CollectedLights;
};
