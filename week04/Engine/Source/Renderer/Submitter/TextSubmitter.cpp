#include "Renderer/Submitter/TextSubmitter.h"

#include "Renderer/D3D11/D3D11TextBatchRenderer.h"
#include "Renderer/SceneRenderData.h"

void FTextSubmitter::Submit(FD3D11TextBatchRenderer& InTextRenderer,
                            const FSceneRenderData&  InSceneRenderData) const
{
    if (InSceneRenderData.SceneView == nullptr ||
        !IsFlagSet(InSceneRenderData.ShowFlags, ESceneShowFlags::SF_BillboardText))
    {
        return;
    }

    InTextRenderer.AddTexts(InSceneRenderData.Texts);
}
