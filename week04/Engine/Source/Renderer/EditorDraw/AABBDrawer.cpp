#include "Renderer/EditorDraw/AABBDrawer.h"

#include "Renderer/D3D11/D3D11LineBatchRenderer.h"

void FAABBDrawer::Draw(FD3D11LineBatchRenderer& InLineRenderer, const Geometry::FAABB& InBounds,
                       const FColor& InColor) const
{
    const FVector Min = InBounds.Min;
    const FVector Max = InBounds.Max;

    const FVector P000(Min.X, Min.Y, Min.Z);
    const FVector P100(Max.X, Min.Y, Min.Z);
    const FVector P010(Min.X, Max.Y, Min.Z);
    const FVector P110(Max.X, Max.Y, Min.Z);

    const FVector P001(Min.X, Min.Y, Max.Z);
    const FVector P101(Max.X, Min.Y, Max.Z);
    const FVector P011(Min.X, Max.Y, Max.Z);
    const FVector P111(Max.X, Max.Y, Max.Z);

    InLineRenderer.AddLine(P000, P100, InColor);
    InLineRenderer.AddLine(P100, P101, InColor);
    InLineRenderer.AddLine(P101, P001, InColor);
    InLineRenderer.AddLine(P001, P000, InColor);

    InLineRenderer.AddLine(P010, P110, InColor);
    InLineRenderer.AddLine(P110, P111, InColor);
    InLineRenderer.AddLine(P111, P011, InColor);
    InLineRenderer.AddLine(P011, P010, InColor);

    InLineRenderer.AddLine(P000, P010, InColor);
    InLineRenderer.AddLine(P100, P110, InColor);
    InLineRenderer.AddLine(P101, P111, InColor);
    InLineRenderer.AddLine(P001, P011, InColor);
}
