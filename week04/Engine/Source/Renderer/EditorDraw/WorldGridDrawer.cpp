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

    const float Extent = static_cast<float>(GridHalfLineCount) * GridSpacing;

    for (int32 i = -GridHalfLineCount; i <= GridHalfLineCount; ++i)
    {
        const float Offset = static_cast<float>(i) * GridSpacing;

        InLineRenderer.AddLine(FVector(-Extent, Offset, 0.0f), FVector(Extent, Offset, 0.0f),
                               GridColor);
        InLineRenderer.AddLine(FVector(Offset, -Extent, 0.0f), FVector(Offset, Extent, 0.0f),
                               GridColor);
    }
}