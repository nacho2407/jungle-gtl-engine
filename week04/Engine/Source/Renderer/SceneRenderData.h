#pragma once

#include "Core/Containers/Array.h"
#include "Renderer/Types/RenderItem.h"
#include "Renderer/Types/ViewMode.h"

class FSceneView;

struct FSceneRenderData
{
    const FSceneView* SceneView = nullptr;
    EViewModeIndex    ViewMode = EViewModeIndex::VMI_Lit;
    bool              bUseInstancing = true;

    TArray<FStaticMeshRenderItem> StaticMeshes;
    TArray<FPrimitiveRenderItem> Primitives;
    TArray<FSpriteRenderItem>    Sprites;
    TArray<FTextRenderItem>      Texts;
};