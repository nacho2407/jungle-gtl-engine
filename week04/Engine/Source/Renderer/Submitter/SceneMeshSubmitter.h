#pragma once

#include "Renderer/SceneRenderData.h"

class FD3D11MeshBatchRenderer;

class FSceneMeshSubmitter
{
  public:
    void Submit(FD3D11MeshBatchRenderer& InMeshRenderer, const FSceneRenderData& InSceneRenderData);
};
