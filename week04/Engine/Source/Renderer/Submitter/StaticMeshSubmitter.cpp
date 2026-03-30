#include "Renderer/Submitter/StaticMeshSubmitter.h"
#include "Renderer/D3D11/D3D11StaticMeshRenderer.h" // 새로 만들 렌더러 헤더
#include "Renderer/SceneRenderData.h"

void FStaticMeshSubmitter::Submit(FD3D11StaticMeshRenderer& InRenderer,
                                  const FSceneRenderData&   InSceneRenderData)
{
    if (InSceneRenderData.SceneView == nullptr || InSceneRenderData.StaticMeshes.empty())
    {
        return;
    }

    InRenderer.AddStaticMeshes(InSceneRenderData.StaticMeshes);
}