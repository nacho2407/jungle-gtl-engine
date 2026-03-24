#include "Renderer/Submitter/PrimitiveSubmitter.h"
#include "Renderer/SceneRenderData.h"
#include "Renderer/D3D11/D3D11MeshBatchRenderer.h"

void FPrimitiveSubmitter::Submit(FD3D11MeshBatchRenderer& InMeshRenderer,
                                 const FSceneRenderData&  InSceneRenderData)
{
    if (InSceneRenderData.SceneView == nullptr ||
        !IsFlagSet(InSceneRenderData.ShowFlags, ESceneShowFlags::SF_Primitives))
    {
        return;
    }
    InMeshRenderer.AddPrimitives(InSceneRenderData.Primitives);
}