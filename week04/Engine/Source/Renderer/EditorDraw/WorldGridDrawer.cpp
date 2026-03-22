#include "Renderer/EditorDraw/WorldGridDrawer.h"

#include "Renderer/D3D11/D3D11LineBatchRenderer.h"
#include "Renderer/EditorRenderData.h"

void FWorldGridDrawer::Draw(FD3D11LineBatchRenderer& InLineRenderer,
                            const FEditorRenderData& InEditorRenderData)
{
    if (!InEditorRenderData.bShowGrid || InEditorRenderData.SceneView == nullptr)
    {
        return;
    }

    InLineRenderer.BeginFrame(InEditorRenderData.SceneView);
    InLineRenderer.AddGrid(GridHalfLineCount, GridSpacing, GridColor);
    InLineRenderer.EndFrame();
}
