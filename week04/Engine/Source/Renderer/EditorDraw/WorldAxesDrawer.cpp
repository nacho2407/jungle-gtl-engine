#include "Renderer/EditorDraw/WorldAxesDrawer.h"

#include "Renderer/D3D11/D3D11LineBatchRenderer.h"
#include "Renderer/EditorRenderData.h"

void FWorldAxesDrawer::Draw(FD3D11LineBatchRenderer& InLineRenderer,
                            const FEditorRenderData& InEditorRenderData)
{
    if (!InEditorRenderData.bShowWorldAxes || InEditorRenderData.SceneView == nullptr)
    {
        return;
    }

    InLineRenderer.BeginFrame(InEditorRenderData.SceneView);
    InLineRenderer.AddWorldAxes(AxisLength);
    InLineRenderer.EndFrame();
}
