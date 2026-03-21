#pragma once

#include "Renderer/Types/ViewMode.h"

class FSceneView;

struct FSceneRenderData
{
    const FSceneView* SceneView = nullptr;

    EViewModeIndex ViewMode = EViewModeIndex::Lit;
};
