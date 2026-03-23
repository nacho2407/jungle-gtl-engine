#pragma once

#include "Renderer/SceneRenderData.h"

class FD3D11MeshBatchRenderer;

class FPrimitiveDrawer
{
  public:
    void Draw(FD3D11MeshBatchRenderer& InMeshRenderer, const FSceneRenderData& InSceneRenderData);
};
