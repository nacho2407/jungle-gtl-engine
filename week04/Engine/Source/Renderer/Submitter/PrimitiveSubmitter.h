#pragma once

#include "Renderer/SceneRenderData.h"

class FD3D11SceneMeshRenderer;

class FPrimitiveSubmitter
{
  public:
    void Submit(FD3D11SceneMeshRenderer& InMeshRenderer, const FSceneRenderData& InSceneRenderData);
};
