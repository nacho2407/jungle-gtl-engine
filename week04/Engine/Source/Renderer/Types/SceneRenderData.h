#pragma once

#include "ViewMode.h"

class FSceneView;

struct FSceneRenderData
{
    const FSceneView *SceneView = nullptr;

    EViewModeIndex ViewMode = EViewModeIndex::Lit;
};
