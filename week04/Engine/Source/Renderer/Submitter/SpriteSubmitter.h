#pragma once

#include "Renderer/SceneRenderData.h"

class FD3D11SpriteBatchRenderer;

class FSpriteSubmitter
{
  public:
    void Submit(FD3D11SpriteBatchRenderer& InSpriteRenderer,
                const FSceneRenderData&     InSceneRenderData) const;
};
