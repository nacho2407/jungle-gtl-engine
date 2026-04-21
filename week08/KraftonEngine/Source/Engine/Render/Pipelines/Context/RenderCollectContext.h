#pragma once

#include "Render/Pipelines/Context/Scene/SceneView.h"

class FScene;
class FViewModePassRegistry;
struct FCollectedPrimitives;

/*
    수집 단계에서만 필요한 얇은 문맥입니다.
    컬렉터가 파이프라인 실행 문맥 전체에 의존하지 않도록,
    현재 뷰/씬/뷰모드 정책과 수집 결과 저장소만 전달합니다.
*/
struct FRenderCollectContext
{
    const FSceneView* SceneView = nullptr;
    FScene* Scene = nullptr;

    const FViewModePassRegistry* ViewModePassRegistry = nullptr;
    EViewMode ActiveViewMode = {};

    FCollectedPrimitives* CollectedPrimitives = nullptr;
};
