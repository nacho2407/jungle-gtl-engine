#pragma once

#include "Render/Execute/Context/Scene/SceneView.h"

class FScene;
class FViewModePassRegistry;
class FRenderer;
struct FCollectedPrimitives;

// FRenderCollectContext는 실행 중 공유되는 상태와 참조를 묶어 전달합니다.
struct FRenderCollectContext
{
    const FSceneView* SceneView = nullptr;
    FScene*           Scene     = nullptr;
    FRenderer*        Renderer  = nullptr;

    const FViewModePassRegistry* ViewModePassRegistry = nullptr;
    EViewMode                    ActiveViewMode       = {};

    FCollectedPrimitives* CollectedPrimitives = nullptr;
};
