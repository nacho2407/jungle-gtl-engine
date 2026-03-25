#include "Renderer/Submitter/PrimitiveSubmitter.h"
#include "Renderer/SceneRenderData.h"
#include "Renderer/D3D11/D3D11SceneMeshRenderer.h"

void FPrimitiveSubmitter::Submit(FD3D11SceneMeshRenderer& InMeshRenderer,
                                 const FSceneRenderData&  InSceneRenderData)
{
    if (InSceneRenderData.SceneView == nullptr || InSceneRenderData.Primitives.empty())
    {
        return;
    }
    InMeshRenderer.AddPrimitives(InSceneRenderData.Primitives);
}