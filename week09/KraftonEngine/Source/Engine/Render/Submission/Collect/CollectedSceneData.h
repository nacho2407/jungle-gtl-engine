// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Render/Execute/Context/RenderCollectContext.h"
#include "Render/Resources/Buffers/LightBufferTypes.h"
#include "Render/Submission/Collect/CollectedOverlayData.h"

class FPrimitiveSceneProxy;

// FCollectedLights는 조명 계산이나 조명 제출에 필요한 데이터를 다룹니다.
struct FCollectedLights
{
    FGlobalLightConstants   GlobalLights;
    TArray<FLocalLightInfo> LocalLights;
};

// FCollectedPrimitives는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FCollectedPrimitives
{
    TArray<FPrimitiveSceneProxy*> VisibleProxies;
    TArray<FPrimitiveSceneProxy*> OpaqueProxies;
    TArray<FPrimitiveSceneProxy*> TransparentProxies;
    TArray<FSceneOverlayText>     OverlayTexts;
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
    const TArray<FPrimitiveSceneProxy*>*   WorldBoundsProxies = nullptr;
};
