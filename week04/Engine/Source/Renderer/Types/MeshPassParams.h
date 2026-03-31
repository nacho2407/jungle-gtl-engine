#pragma once

#include "Renderer/Types/ViewMode.h"

class FSceneView;

struct FMeshPassParams
{
    const class FSceneView* SceneView = nullptr;
    EViewModeIndex          ViewMode = EViewModeIndex::VMI_Lit;
    bool                    bUseInstancing = false;
    bool                    bDisableDepth = false;
    float                   Time = 0.0f; // 누적 시간 추가
};