#pragma once

#include "Renderer/D3D11/D3D11MeshBatchRenderer.h"

class FD3D11SceneMeshRenderer
{
  public:
    bool Initialize(FD3D11RHI* InRHI);
    void Shutdown();

    void BeginFrame(const FSceneView* InSceneView, EViewModeIndex InViewMode,
                    bool bInUseInstancing);
    void AddPrimitive(const FPrimitiveRenderItem& InItem);
    void AddPrimitives(const TArray<FPrimitiveRenderItem>& InItems);
    void EndFrame();
    void Flush();

  private:
    FD3D11MeshBatchRenderer MeshBatchRenderer;
};
