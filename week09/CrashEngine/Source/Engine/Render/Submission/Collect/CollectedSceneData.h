#pragma once

#include "Render/Execute/Context/RenderCollectContext.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Submission/Collect/CollectedOverlayData.h"

class FPrimitiveProxy;

// FCollectedLights는 조명 계산이나 조명 제출에 필요한 데이터를 다룹니다.
struct FCollectedLights
{
    FGlobalLightCBData        GlobalLights;
    TArray<FLocalLightCBData> LocalLights;
    TArray<class FLightProxy*> VisibleLightProxies;
};

// FCollectedPrimitives는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FCollectedPrimitives
{
    TArray<FPrimitiveProxy*>  VisibleProxies;
    TArray<FPrimitiveProxy*>  OpaqueProxies;
    TArray<FPrimitiveProxy*>  TransparentProxies;
    TArray<FSceneOverlayText> OverlayTexts;
};

// FCollectedSceneData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FCollectedSceneData
{
    FCollectedPrimitives Primitives;
    FCollectedLights     Lights;
};

// FCollectOverlayContext는 실행 중 공유되는 상태와 참조를 묶어 전달합니다.
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
    const TArray<FPrimitiveProxy*>*        WorldBoundsProxies = nullptr;
};
