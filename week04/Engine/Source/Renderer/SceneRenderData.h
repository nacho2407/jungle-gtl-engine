#pragma once

#include "Core/Containers/Array.h"
#include "Renderer/Types/RenderItem.h"
#include "Renderer/Types/SceneShowFlags.h"
#include "Renderer/Types/ViewMode.h"

class FSceneView;

struct FSceneRenderData
{
    const FSceneView* SceneView = nullptr;
    EViewModeIndex    ViewMode = EViewModeIndex::VMI_Lit;

    ESceneShowFlags ShowFlags = ESceneShowFlags::SF_Primitives | ESceneShowFlags::SF_BillboardText |
                                ESceneShowFlags::SF_Sprites | ESceneShowFlags::SF_UUIDText;

    bool bUseInstancing = true;

    TArray<FPrimitiveRenderItem> Primitives;
    TArray<FSpriteRenderItem>    Sprites;
    TArray<FTextRenderItem>      Texts;
};