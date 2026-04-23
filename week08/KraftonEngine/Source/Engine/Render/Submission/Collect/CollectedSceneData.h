#pragma once

#include "Render/Execute/Context/RenderCollectContext.h"
#include "Render/Resources/Buffers/LightBufferTypes.h"
#include "Render/Submission/Collect/CollectedOverlayData.h"

class FPrimitiveSceneProxy;

struct FCollectedLights
{
    FGlobalLightConstants   GlobalLights;
    TArray<FLocalLightInfo> LocalLights;
};

struct FCollectedPrimitives
{
    TArray<FPrimitiveSceneProxy*> VisibleProxies;
    TArray<FPrimitiveSceneProxy*> OpaqueProxies;
    TArray<FPrimitiveSceneProxy*> TransparentProxies;
    TArray<FSceneOverlayText>     OverlayTexts;
};

struct FCollectedSceneData
{
    FCollectedPrimitives Primitives;
    FCollectedLights     Lights;
};

struct FCollectOverlayContext
{
    const class FOverlayStatSystem*        OverlaySystem      = nullptr;
    const class UEditorEngine*             Editor             = nullptr;
    const struct FSceneView*               SceneView          = nullptr;
    const class FScene*                    Scene              = nullptr;
    const class UWorld*                    World              = nullptr;
    float                                  GridSpacing        = 0.0f;
    int32                                  GridHalfLineCount  = 0;
    const class FOctree*                   Octree             = nullptr;
    const class FWorldPrimitivePickingBVH* WorldBVH           = nullptr;
    const TArray<FPrimitiveSceneProxy*>*   WorldBoundsProxies = nullptr;
};
