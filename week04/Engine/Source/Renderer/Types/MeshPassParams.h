#pragma once

#include "Renderer/Types/ViewMode.h"

class FSceneView;

enum class ERasterizerCullMode : uint8
{
    CULL_None,
    CULL_Back,
    CULL_Front,
};

struct FMeshPassParams
{
    const class FSceneView* SceneView = nullptr;
    EViewModeIndex          ViewMode = EViewModeIndex::VMI_Lit;
    ERasterizerCullMode     CullMode = ERasterizerCullMode::CULL_Back;
    bool                    bUseInstancing = false;
    bool                    bDisableDepth = false;
    float                   Time = 0.0f; // 누적 시간 추가
};