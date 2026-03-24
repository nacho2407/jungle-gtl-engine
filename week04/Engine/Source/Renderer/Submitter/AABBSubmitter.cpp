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

    for (const FSpriteRenderItem& Item : InSceneRenderData.Sprites)
    {
        SubmitSpriteBounds(InLineRenderer, *InSceneRenderData.SceneView, Item);
    }

    for (const FTextRenderItem& Item : InSceneRenderData.Texts)
    {
        SubmitTextBounds(InLineRenderer, *InSceneRenderData.SceneView, Item);
    }
}

FColor FAABBSubmitter::ResolveBoundsColor(const FRenderItemState& InState)
{
    if (InState.IsSelected())
    {
        return FColor(0.1f, 0.4f, 1.0f, 1.0f);
    }

    if (InState.IsHovered())
    {
        return FColor(1.0f, 0.9f, 0.1f, 1.0f);
    }

    return FColor::White();
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

    const FVector Origin = InItem.World.GetOrigin();
    const FVector Right = InItem.World.GetRightVector() * MakeHalfExtent(InItem.MeshType).X;
    const FVector Up = InItem.World.GetUpVector() * MakeHalfExtent(InItem.MeshType).Z;
    const FVector Forward = InItem.World.GetForwardVector() * MakeHalfExtent(InItem.MeshType).Y;

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

void FAABBSubmitter::SubmitSpriteBounds(FD3D11LineBatchRenderer& InLineRenderer,
                                        const FSceneView&        InSceneView,
                                        const FSpriteRenderItem& InItem)
{
    if (!InItem.State.IsVisible() || !InItem.State.bShowBounds || InItem.TextureResource == nullptr)
    {
        return;
    }

    const FMatrix& PlacementWorld = InItem.Placement.World;
    const FVector  Origin = PlacementWorld.GetOrigin() + InItem.Placement.WorldOffset;

    FVector RightAxis;
    FVector UpAxis;

    if (InItem.Placement.IsBillboard())
    {
        const FMatrix CameraWorld = InSceneView.GetViewMatrix().GetInverse();
        RightAxis = CameraWorld.GetRightVector() * PlacementWorld.GetScaleVector().X;
        UpAxis = CameraWorld.GetUpVector() * PlacementWorld.GetScaleVector().Z;
    }
    else
    {
        RightAxis = PlacementWorld.GetRightVector();
        UpAxis = PlacementWorld.GetUpVector();
    }

    FVector Min(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    AddQuadBounds(Min, Max, Origin, RightAxis, UpAxis);
    SubmitBox(InLineRenderer, Min, Max, ResolveBoundsColor(InItem.State));
}

void FAABBSubmitter::SubmitTextBounds(FD3D11LineBatchRenderer& InLineRenderer,
                                      const FSceneView& InSceneView, const FTextRenderItem& InItem)
{
    if (!InItem.State.IsVisible() || !InItem.State.bShowBounds || InItem.FontResource == nullptr ||
        InItem.Text.empty())
    {
        return;
    }

    const FFontResource& Font = *InItem.FontResource;
    const float          Scale = InItem.TextScale;
    const float          LetterSpacing = InItem.LetterSpacing;
    const float          LineSpacing = InItem.LineSpacing;
    const float          LineHeight =
        (Font.Common.LineHeight > 0) ? static_cast<float>(Font.Common.LineHeight) : 16.0f;

    float MaxWidth = 0.0f;
    float CurrentWidth = 0.0f;
    int32 LineCount = 1;

    for (char Ch : InItem.Text)
    {
        if (Ch == '\r')
        {
            continue;
        }

        if (Ch == '\n')
        {
            if (CurrentWidth > MaxWidth)
            {
                MaxWidth = CurrentWidth;
            }
            CurrentWidth = 0.0f;
            ++LineCount;
            continue;
        }

        const uint32 CodePoint = static_cast<uint8>(Ch);
        const FFontGlyph* Glyph = Font.FindGlyph(CodePoint);
        if (Glyph == nullptr)
        {
            CurrentWidth += Scale * (LineHeight * 0.5f + LetterSpacing);
            continue;
        }

        CurrentWidth += Scale * (static_cast<float>(Glyph->XAdvance) + LetterSpacing);
    }

    if (CurrentWidth > MaxWidth)
    {
        MaxWidth = CurrentWidth;
    }

    const float TotalHeight = static_cast<float>(LineCount) * LineHeight * Scale +
                              static_cast<float>(LineCount - 1) * LineSpacing * Scale;

    const FMatrix& PlacementWorld = InItem.Placement.World;
    const FVector  Origin = PlacementWorld.GetOrigin() + InItem.Placement.WorldOffset;

    FVector RightAxis;
    FVector UpAxis;

    if (InItem.Placement.IsBillboard())
    {
        const FMatrix CameraWorld = InSceneView.GetViewMatrix().GetInverse();
        RightAxis = CameraWorld.GetRightVector();
        UpAxis = CameraWorld.GetUpVector();
    }
    else
    {
        RightAxis = PlacementWorld.GetRightVector();
        UpAxis = PlacementWorld.GetUpVector();
    }

    const FVector Center = Origin + RightAxis * (MaxWidth * 0.5f) - UpAxis * (TotalHeight * 0.5f);
    const FVector HalfRight = RightAxis * (MaxWidth * 0.5f);
    const FVector HalfUp = UpAxis * (TotalHeight * 0.5f);

    FVector Min(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    AddQuadBounds(Min, Max, Center, HalfRight, HalfUp);
    SubmitBox(InLineRenderer, Min, Max, ResolveBoundsColor(InItem.State));
}
