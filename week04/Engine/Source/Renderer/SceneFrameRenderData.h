#pragma once

#include "Core/CoreMinimal.h"

#include "Renderer/Types/RenderItem.h"

// View-independent scene items built once per frame and stored by the renderer.
// Passed to FRendererModule::SetSceneFrameData before the per-panel render loop.
struct FSceneFrameRenderData
{
    bool bUseInstancing = true;

    TArray<FStaticMeshRenderItem> StaticMeshes;
    TArray<FPrimitiveRenderItem> Primitives;
    TArray<FSpriteRenderItem>    Sprites;
    TArray<FTextRenderItem>      Texts;
};
