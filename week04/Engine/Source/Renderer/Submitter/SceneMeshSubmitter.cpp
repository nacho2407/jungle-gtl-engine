#include "Renderer/Submitter/SceneMeshSubmitter.h"

#include "Renderer/D3D11/D3D11MeshBatchRenderer.h"
#include "Renderer/SceneRenderData.h"

void FSceneMeshSubmitter::Submit(FD3D11MeshBatchRenderer& InMeshRenderer,
                                 const FSceneRenderData&  InSceneRenderData)
{
    if (InSceneRenderData.SceneView == nullptr || InSceneRenderData.Primitives.empty())
    {
        return;
    }

    InMeshRenderer.AddPrimitives(InSceneRenderData.Primitives);
}
