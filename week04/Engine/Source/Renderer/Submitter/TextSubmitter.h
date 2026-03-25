#pragma once

#include "Renderer/SceneRenderData.h"

class FD3D11LineBatchRenderer;
class FD3D11TextBatchRenderer;

class FTextSubmitter
{
  public:
    void Submit(FD3D11TextBatchRenderer& InTextRenderer,
                const FSceneRenderData&   InSceneRenderData) const;

    void Submit(FD3D11LineBatchRenderer& InLineRenderer,
                const FSceneRenderData&   InSceneRenderData) const;
};
