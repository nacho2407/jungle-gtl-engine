#pragma once

#include "Render/Pipelines/Context/RenderCollectContext.h"
#include "Render/Scene/Proxies/Light/LightTypes.h"
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

/*
    FCollectedSceneData는 Scene 수집 결과를 한 덩어리로 묶는 구조체입니다.
    Primitive와 Light를 Renderer가 따로 들고 있지 않고 collector 결과 하나로 전달합니다.
*/
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
    float                                  GridSpacing        = 0.0f;
    int32                                  GridHalfLineCount  = 0;
    const class FOctree*                   Octree             = nullptr;
    const class FWorldPrimitivePickingBVH* WorldBVH           = nullptr;
    const TArray<FPrimitiveSceneProxy*>*   WorldBoundsProxies = nullptr;
};
