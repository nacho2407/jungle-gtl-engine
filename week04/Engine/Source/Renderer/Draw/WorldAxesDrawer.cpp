#include "Renderer/Draw/WorldAxesDrawer.h"

#include "Renderer/D3D11/D3D11LineBatchRenderer.h"
#include "Renderer/EditorRenderData.h"
#include "Renderer/Types/AxisColors.h"

void FWorldAxesDrawer::Draw(FD3D11LineBatchRenderer& InLineRenderer,
                            const FEditorRenderData& InEditorRenderData)
{
    if (!IsFlagSet(InEditorRenderData.ShowFlags, EEditorShowFlags::SF_WorldAxes) ||
        InEditorRenderData.SceneView == nullptr)
    {
        return;
    }

    constexpr float AxisExtent = 1000.0f;

    InLineRenderer.AddLine(FVector(-AxisExtent, 0.0f, 0.0f), FVector(AxisExtent, 0.0f, 0.0f),
                           GetAxisBaseColor(EAxis::X));
    InLineRenderer.AddLine(FVector(0.0f, -AxisExtent, 0.0f), FVector(0.0f, AxisExtent, 0.0f),
                           GetAxisBaseColor(EAxis::Y));
    InLineRenderer.AddLine(FVector(0.0f, 0.0f, -AxisExtent), FVector(0.0f, 0.0f, AxisExtent),
                           GetAxisBaseColor(EAxis::Z));
}
