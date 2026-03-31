#pragma once

#include "Renderer/SceneRenderData.h"

class FD3D11StaticMeshRenderer;

class FStaticMeshSubmitter
{
  public:
    void Submit(FD3D11StaticMeshRenderer& InRenderer, const FSceneRenderData& InSceneRenderData);
};