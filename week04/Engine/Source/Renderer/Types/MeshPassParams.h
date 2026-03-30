#pragma once

#include "Renderer/Types/ViewMode.h"

class FSceneView;

struct FMeshPassParams
{
    const FSceneView* SceneView = nullptr;
    EViewModeIndex    ViewMode = EViewModeIndex::VMI_Lit;
    bool              bUseInstancing = true;
    bool              bDisableDepth = false;
};