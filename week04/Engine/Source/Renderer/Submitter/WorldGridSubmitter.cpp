#include "Renderer/Submitter/WorldGridSubmitter.h"

#include "Engine/EngineStatics.h"
#include "Renderer/D3D11/D3D11LineBatchRenderer.h"
#include "Renderer/EditorRenderData.h"

void FWorldGridSubmitter::Submit(FD3D11LineBatchRenderer& InLineRenderer,
                                 const FEditorRenderData& InEditorRenderData)
{
    if (InEditorRenderData.SceneView == nullptr)
    {
        return;
    }

    const float Extent = static_cast<float>(GridHalfLineCount) * UEngineStatics::GridSpacing;

    for (int32 i = -GridHalfLineCount; i <= GridHalfLineCount; ++i)
    {
        if (InEditorRenderData.bShowWorldAxes && i == 0)
        {
            continue;
        }

        const float   Offset = static_cast<float>(i) * UEngineStatics::GridSpacing;
        const bool    bIsMajorLine = (i % MajorLineEvery) == 0;
        const FColor& LineColor = bIsMajorLine ? MajorGridColor : MinorGridColor;

        InLineRenderer.AddLine(FVector(-Extent, Offset, 0.0f), FVector(Extent, Offset, 0.0f),
                               LineColor);
        InLineRenderer.AddLine(FVector(Offset, -Extent, 0.0f), FVector(Offset, Extent, 0.0f),
                               LineColor);
    }
}
