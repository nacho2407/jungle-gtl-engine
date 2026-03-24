#pragma once

#include "Core/Containers/Array.h"
#include "Renderer/Types/RenderItem.h"
#include "Renderer/Types/SceneShowFlags.h"
#include "Renderer/Types/ViewMode.h"

class FSceneView;

struct FSceneRenderData
{
    const FSceneView* SceneView = nullptr;
    EViewModeIndex    ViewMode = EViewModeIndex::Lit;

    ESceneShowFlags ShowFlags = ESceneShowFlags::SF_Primitives | ESceneShowFlags::SF_BillboardText;

    bool bUseInstancing = true;

    TArray<FPrimitiveRenderItem> Primitives;
    TArray<FTextRenderItem>      Texts;
};