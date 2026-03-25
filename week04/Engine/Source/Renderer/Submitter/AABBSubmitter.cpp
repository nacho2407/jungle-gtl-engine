#include <cfloat>

#include "Renderer/Submitter/AABBSubmitter.h"

#include "Renderer/D3D11/D3D11LineBatchRenderer.h"
#include "Renderer/RenderAsset/FontResource.h"
#include "Renderer/SceneView.h"

namespace
{
    void ExpandLocal(FVector& InOutMin, FVector& InOutMax, const FVector& InPoint)
    {
        if (InPoint.X < InOutMin.X)
            InOutMin.X = InPoint.X;
        if (InPoint.Y < InOutMin.Y)
            InOutMin.Y = InPoint.Y;
        if (InPoint.Z < InOutMin.Z)
            InOutMin.Z = InPoint.Z;
        if (InPoint.X > InOutMax.X)
            InOutMax.X = InPoint.X;
        if (InPoint.Y > InOutMax.Y)
            InOutMax.Y = InPoint.Y;
        if (InPoint.Z > InOutMax.Z)
            InOutMax.Z = InPoint.Z;
    }

    FVector MakeHalfExtent(EBasicMeshType InMeshType)
    {
        switch (InMeshType)
        {
        case EBasicMeshType::Quad:
            return FVector(0.5f, 0.05f, 0.5f);
        case EBasicMeshType::Ring:
            return FVector(0.5f, 0.5f, 0.05f);
        case EBasicMeshType::Triangle:
            return FVector(0.5f, 0.05f, 0.5f);
        default:
            return FVector(0.5f, 0.5f, 0.5f);
        }
    }

    void AddQuadBounds(FVector& InOutMin, FVector& InOutMax, const FVector& InOrigin,
                       const FVector& InRight, const FVector& InUp)
    {
        const FVector P0 = InOrigin - InRight - InUp;
        const FVector P1 = InOrigin + InRight - InUp;
        const FVector P2 = InOrigin + InRight + InUp;
        const FVector P3 = InOrigin - InRight + InUp;

        ExpandLocal(InOutMin, InOutMax, P0);
        ExpandLocal(InOutMin, InOutMax, P1);
        ExpandLocal(InOutMin, InOutMax, P2);
        ExpandLocal(InOutMin, InOutMax, P3);
    }
} // namespace

void FAABBSubmitter::Submit(FD3D11LineBatchRenderer& InLineRenderer,
                            const FSceneRenderData&  InSceneRenderData) const
{
    if (InSceneRenderData.SceneView == nullptr)
    {
        return;
    }

    for (const FPrimitiveRenderItem& Item : InSceneRenderData.Primitives)
    {
        SubmitPrimitiveBounds(InLineRenderer, Item);
    }
}

FColor FAABBSubmitter::ResolveBoundsColor(const FRenderItemState& InState)
{
    if (InState.IsSelected())
    {
        return SelectedBoundsColor;
    }

    if (InState.IsHovered())
    {
        return HoveredBoundsColor;
    }

    return DefaultBoundsColor;
}

void FAABBSubmitter::ExpandBounds(FVector& InOutMin, FVector& InOutMax, const FVector& InPoint)
{
    if (InPoint.X < InOutMin.X)
        InOutMin.X = InPoint.X;
    if (InPoint.Y < InOutMin.Y)
        InOutMin.Y = InPoint.Y;
    if (InPoint.Z < InOutMin.Z)
        InOutMin.Z = InPoint.Z;
    if (InPoint.X > InOutMax.X)
        InOutMax.X = InPoint.X;
    if (InPoint.Y > InOutMax.Y)
        InOutMax.Y = InPoint.Y;
    if (InPoint.Z > InOutMax.Z)
        InOutMax.Z = InPoint.Z;
}

void FAABBSubmitter::SubmitBox(FD3D11LineBatchRenderer& InLineRenderer, const FVector& InMin,
                               const FVector& InMax, const FColor& InColor)
{
    const FVector P000(InMin.X, InMin.Y, InMin.Z);
    const FVector P100(InMax.X, InMin.Y, InMin.Z);
    const FVector P010(InMin.X, InMax.Y, InMin.Z);
    const FVector P110(InMax.X, InMax.Y, InMin.Z);
    const FVector P001(InMin.X, InMin.Y, InMax.Z);
    const FVector P101(InMax.X, InMin.Y, InMax.Z);
    const FVector P011(InMin.X, InMax.Y, InMax.Z);
    const FVector P111(InMax.X, InMax.Y, InMax.Z);

    InLineRenderer.AddLine(P000, P100, InColor);
    InLineRenderer.AddLine(P100, P110, InColor);
    InLineRenderer.AddLine(P110, P010, InColor);
    InLineRenderer.AddLine(P010, P000, InColor);

    InLineRenderer.AddLine(P001, P101, InColor);
    InLineRenderer.AddLine(P101, P111, InColor);
    InLineRenderer.AddLine(P111, P011, InColor);
    InLineRenderer.AddLine(P011, P001, InColor);

    InLineRenderer.AddLine(P000, P001, InColor);
    InLineRenderer.AddLine(P100, P101, InColor);
    InLineRenderer.AddLine(P110, P111, InColor);
    InLineRenderer.AddLine(P010, P011, InColor);
}

void FAABBSubmitter::SubmitPrimitiveBounds(FD3D11LineBatchRenderer&    InLineRenderer,
                                           const FPrimitiveRenderItem& InItem)
{
    if (!InItem.State.IsVisible() || !InItem.State.bShowBounds)
    {
        return;
    }

    if (InItem.bHasWorldAABB)
    {
        SubmitBox(InLineRenderer, InItem.WorldAABB.Min, InItem.WorldAABB.Max,
                  ResolveBoundsColor(InItem.State));
        return;
    }

    const FVector Origin = InItem.World.GetOrigin();
    const FVector Right = InItem.World.GetScaledAxis(EAxis::Y) * MakeHalfExtent(InItem.MeshType).X;
    const FVector Up = InItem.World.GetScaledAxis(EAxis::Z) * MakeHalfExtent(InItem.MeshType).Z;
    const FVector Forward =
        InItem.World.GetScaledAxis(EAxis::X) * MakeHalfExtent(InItem.MeshType).Y;

    FVector Min(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    const FVector Signs[8] = {
        Right + Up + Forward,  Right + Up - Forward,  Right - Up + Forward,  Right - Up - Forward,
        -Right + Up + Forward, -Right + Up - Forward, -Right - Up + Forward, -Right - Up - Forward,
    };

    for (const FVector& CornerOffset : Signs)
    {
        ExpandBounds(Min, Max, Origin + CornerOffset);
    }

    SubmitBox(InLineRenderer, Min, Max, ResolveBoundsColor(InItem.State));
}