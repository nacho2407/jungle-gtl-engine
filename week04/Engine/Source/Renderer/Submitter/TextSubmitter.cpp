#include "Renderer/Submitter/TextSubmitter.h"

#include "Renderer/D3D11/D3D11LineBatchRenderer.h"
#include "Renderer/D3D11/D3D11TextBatchRenderer.h"
#include "Renderer/RenderAsset/FontResource.h"
#include "Renderer/SceneRenderData.h"
#include "Renderer/SceneView.h"
#include "Renderer/Types/RenderDebugColors.h"

#include <algorithm>

namespace
{
    constexpr float DefaultLineHeight = 16.0f;

    struct FLaidOutGlyph
    {
        const FFontGlyph* Glyph = nullptr;

        float MinX = 0.0f;
        float MinY = 0.0f;
        float MaxX = 0.0f;
        float MaxY = 0.0f;

        bool   bSolidColorQuad = false;
        FColor SolidColor = FColor::White();
    };

    struct FTextLayout
    {
        TArray<FLaidOutGlyph> Glyphs;

        float MinX = 0.0f;
        float MinY = 0.0f;
        float MaxX = 0.0f;
        float MaxY = 0.0f;

        float GetWidth() const { return MaxX - MinX; }
        float GetHeight() const { return MaxY - MinY; }
        bool  HasGlyphs() const { return !Glyphs.empty(); }
        bool  IsValid() const { return HasGlyphs() && GetWidth() > 0.0f && GetHeight() > 0.0f; }
    };

    struct FResolvedGlyph
    {
        const FFontGlyph* Glyph = nullptr;
        bool              bMissing = true;
    };

    void SubmitQuadOutline(FD3D11LineBatchRenderer& InLineRenderer, const FVector& InBottomLeft,
                           const FVector& InRight, const FVector& InUp, const FColor& InColor)
    {
        const FVector P0 = InBottomLeft;
        const FVector P1 = InBottomLeft + InRight;
        const FVector P2 = InBottomLeft + InRight + InUp;
        const FVector P3 = InBottomLeft + InUp;

        InLineRenderer.AddLine(P0, P1, InColor);
        InLineRenderer.AddLine(P1, P2, InColor);
        InLineRenderer.AddLine(P2, P3, InColor);
        InLineRenderer.AddLine(P3, P0, InColor);
    }

    bool ShouldSubmitAnyText(const FSceneRenderData& InSceneRenderData)
    {
        return InSceneRenderData.SceneView != nullptr &&
               (IsFlagSet(InSceneRenderData.ShowFlags, ESceneShowFlags::SF_BillboardText) ||
                IsFlagSet(InSceneRenderData.ShowFlags, ESceneShowFlags::SF_UUIDText));
    }

    bool ShouldRenderTextItem(const FSceneRenderData& InSceneRenderData,
                              const FTextRenderItem&  InItem)
    {
        if (!InItem.State.IsVisible() || InItem.Text.empty())
        {
            return false;
        }

        if (InItem.bIsUUIDText)
        {
            return IsFlagSet(InSceneRenderData.ShowFlags, ESceneShowFlags::SF_UUIDText);
        }

        return IsFlagSet(InSceneRenderData.ShowFlags, ESceneShowFlags::SF_BillboardText);
    }

    FResolvedGlyph ResolveGlyph(const FFontResource& InFont, uint32 InCodePoint)
    {
        if (const FFontGlyph* Glyph = InFont.FindGlyph(InCodePoint))
        {
            if (Glyph->IsValid())
            {
                return {Glyph, false};
            }
        }

        if (const FFontGlyph* QuestionGlyph = InFont.FindGlyph(static_cast<uint32>('?')))
        {
            if (QuestionGlyph->IsValid())
            {
                return {QuestionGlyph, false};
            }
        }

        return {nullptr, true};
    }

    float GetMissingGlyphAdvance(const FFontResource& InFont, float InLineHeight, float InScale)
    {
        if (const FFontGlyph* SpaceGlyph = InFont.FindGlyph(static_cast<uint32>(' ')))
        {
            if (SpaceGlyph->IsValid())
            {
                return static_cast<float>(SpaceGlyph->XAdvance) * InScale;
            }
        }

        return InLineHeight * 0.5f * InScale;
    }

    FTextLayout BuildTextLayout(const FTextRenderItem& InItem)
    {
        FTextLayout Layout;

        if (InItem.FontResource == nullptr || InItem.Text.empty())
        {
            return Layout;
        }

        const float RawLineHeight = (InItem.FontResource->Common.LineHeight > 0)
                                        ? static_cast<float>(InItem.FontResource->Common.LineHeight)
                                        : DefaultLineHeight;

        const float UnitScale =
            (RawLineHeight > 0.0f) ? (1.0f / RawLineHeight) : (1.0f / DefaultLineHeight);

        const float Scale = (InItem.TextScale > 0.0f) ? (InItem.TextScale * UnitScale) : UnitScale;
        const float LetterSpacing = InItem.LetterSpacing * UnitScale;
        const float LineSpacing = InItem.LineSpacing * UnitScale;

        const float MissingAdvance =
            GetMissingGlyphAdvance(*InItem.FontResource, RawLineHeight, Scale);
        const float MissingWidth = MissingAdvance;
        const float MissingHeight = RawLineHeight * Scale;

        float PenX = 0.0f;
        float PenY = 0.0f;
        bool  bHasBounds = false;

        for (char Ch : InItem.Text)
        {
            if (Ch == '\r')
            {
                continue;
            }

            if (Ch == '\n')
            {
                PenX = 0.0f;
                PenY += RawLineHeight * Scale + LineSpacing;
                continue;
            }

            if (Ch == ' ')
            {
                float SpaceAdvance = 0.0f;

                if (const FFontGlyph* SpaceGlyph =
                        InItem.FontResource->FindGlyph(static_cast<uint32>(' ')))
                {
                    if (SpaceGlyph->IsValid())
                    {
                        SpaceAdvance = static_cast<float>(SpaceGlyph->XAdvance) * Scale;
                    }
                }

                if (SpaceAdvance <= 0.0f)
                {
                    SpaceAdvance = RawLineHeight * 0.25f * Scale;
                }

                PenX += SpaceAdvance;
                continue;
            }

            const uint32         CodePoint = static_cast<uint8>(Ch);
            const FResolvedGlyph Resolved = ResolveGlyph(*InItem.FontResource, CodePoint);

            FLaidOutGlyph OutGlyph;

            if (Resolved.bMissing)
            {
                OutGlyph.Glyph = nullptr;
                OutGlyph.bSolidColorQuad = true;
                OutGlyph.SolidColor = RenderDebugColors::MissingGlyph;

                OutGlyph.MinX = PenX;
                OutGlyph.MinY = PenY;
                OutGlyph.MaxX = PenX + MissingWidth;
                OutGlyph.MaxY = PenY + MissingHeight;

                PenX += MissingAdvance + LetterSpacing;
            }
            else
            {
                const FFontGlyph* Glyph = Resolved.Glyph;

                OutGlyph.Glyph = Glyph;
                OutGlyph.bSolidColorQuad = false;
                OutGlyph.MinX = PenX + static_cast<float>(Glyph->XOffset) * Scale;
                OutGlyph.MinY = PenY + static_cast<float>(Glyph->YOffset) * Scale;
                OutGlyph.MaxX = OutGlyph.MinX + static_cast<float>(Glyph->Width) * Scale;
                OutGlyph.MaxY = OutGlyph.MinY + static_cast<float>(Glyph->Height) * Scale;

                PenX += Scale * static_cast<float>(Glyph->XAdvance);
                PenX += LetterSpacing;
            }

            Layout.Glyphs.push_back(OutGlyph);

            if (!bHasBounds)
            {
                Layout.MinX = OutGlyph.MinX;
                Layout.MinY = OutGlyph.MinY;
                Layout.MaxX = OutGlyph.MaxX;
                Layout.MaxY = OutGlyph.MaxY;
                bHasBounds = true;
            }
            else
            {
                Layout.MinX = std::min(Layout.MinX, OutGlyph.MinX);
                Layout.MinY = std::min(Layout.MinY, OutGlyph.MinY);
                Layout.MaxX = std::max(Layout.MaxX, OutGlyph.MaxX);
                Layout.MaxY = std::max(Layout.MaxY, OutGlyph.MaxY);
            }
        }

        return Layout;
    }

    void BuildPlacementAxes(const FSceneView& InSceneView, const FRenderPlacement& InPlacement,
                            FVector& OutOrigin, FVector& OutRightAxis, FVector& OutUpAxis)
    {
        const FMatrix& PlacementWorld = InPlacement.World;
        OutOrigin = PlacementWorld.GetOrigin() + InPlacement.WorldOffset;

        if (InPlacement.IsBillboard())
        {
            const FMatrix CameraWorld = InSceneView.GetViewMatrix().GetInverse();
            OutRightAxis = CameraWorld.GetRightVector().GetSafeNormal();
            OutUpAxis = CameraWorld.GetUpVector().GetSafeNormal();
        }
        else
        {
            OutRightAxis = PlacementWorld.GetRightVector().GetSafeNormal();
            OutUpAxis = PlacementWorld.GetUpVector().GetSafeNormal();
        }
    }
} // namespace

void FTextSubmitter::Submit(FD3D11TextBatchRenderer& InTextRenderer,
                            const FSceneRenderData&  InSceneRenderData) const
{
    if (!ShouldSubmitAnyText(InSceneRenderData))
    {
        return;
    }

    TArray<FTextRenderItem> FilteredTexts;
    for (const FTextRenderItem& Item : InSceneRenderData.Texts)
    {
        if (ShouldRenderTextItem(InSceneRenderData, Item))
        {
            FilteredTexts.push_back(Item);
        }
    }

    if (!FilteredTexts.empty())
    {
        InTextRenderer.AddTexts(FilteredTexts);
    }
}

void FTextSubmitter::Submit(FD3D11LineBatchRenderer& InLineRenderer,
                            const FSceneRenderData&  InSceneRenderData) const
{
    if (!ShouldSubmitAnyText(InSceneRenderData))
    {
        return;
    }

    const FSceneView* SceneView = InSceneRenderData.SceneView;
    if (SceneView == nullptr)
    {
        return;
    }

    for (const FTextRenderItem& Item : InSceneRenderData.Texts)
    {
        if (!ShouldRenderTextItem(InSceneRenderData, Item))
        {
            continue;
        }

        if (Item.FontResource == nullptr)
        {
            FVector Origin;
            FVector RightAxis;
            FVector UpAxis;
            BuildPlacementAxes(*SceneView, Item.Placement, Origin, RightAxis, UpAxis);

            float Width = 1.0f;
            float Height = 1.0f;

            if (Item.LayoutMode == ETextLayoutMode::FitToBox)
            {
                const FVector WorldScale = Item.Placement.World.GetScaleVector();
                Width = std::max(WorldScale.X, 1.0f);
                Height = std::max(WorldScale.Y, 1.0f);
            }
            else
            {
                const float FallbackExtent = std::max(Item.TextScale, 1.0f);
                Width = FallbackExtent;
                Height = FallbackExtent;
            }

            const FVector QuadRight = RightAxis * Width;
            const FVector QuadUp = UpAxis * Height;
            const FVector BottomLeft = Origin - QuadRight * 0.5f - QuadUp * 0.5f;

            SubmitQuadOutline(InLineRenderer, BottomLeft, QuadRight, QuadUp,
                              RenderDebugColors::MissingGlyph);
            continue;
        }

        const FTextLayout Layout = BuildTextLayout(Item);
        if (!Layout.IsValid())
        {
            continue;
        }

        FVector TextOrigin;
        FVector RightAxis;
        FVector UpAxis;
        BuildPlacementAxes(*SceneView, Item.Placement, TextOrigin, RightAxis, UpAxis);

        if (Item.LayoutMode == ETextLayoutMode::FitToBox)
        {
            const FVector WorldScale = Item.Placement.World.GetScaleVector();
            const float   BoxWidth = std::max(WorldScale.X, 1.0f);
            const float   BoxHeight = std::max(WorldScale.Y, 1.0f);

            const float LayoutWidth = Layout.GetWidth();
            const float LayoutHeight = Layout.GetHeight();
            const float UniformScale = std::min(BoxWidth / LayoutWidth, BoxHeight / LayoutHeight);

            const float FinalWidth = LayoutWidth * UniformScale;
            const float FinalHeight = LayoutHeight * UniformScale;
            const float OffsetX = (BoxWidth - FinalWidth) * 0.5f;
            const float OffsetY = (BoxHeight - FinalHeight) * 0.5f;

            for (const FLaidOutGlyph& G : Layout.Glyphs)
            {
                const float X0 = OffsetX + (G.MinX - Layout.MinX) * UniformScale;
                const float Y0 = OffsetY + (G.MinY - Layout.MinY) * UniformScale;
                const float W = (G.MaxX - G.MinX) * UniformScale;
                const float H = (G.MaxY - G.MinY) * UniformScale;

                const FVector BottomLeft = TextOrigin + RightAxis * X0 - UpAxis * Y0;
                const FVector GlyphRight = RightAxis * W;
                const FVector GlyphUp = UpAxis * H;
                const FColor  OutlineColor = G.bSolidColorQuad ? G.SolidColor : Item.Color;

                SubmitQuadOutline(InLineRenderer, BottomLeft, GlyphRight, -GlyphUp, OutlineColor);
            }
        }
        else
        {
            const float CenterX = (Layout.MinX + Layout.MaxX) * 0.5f;
            const float CenterY = (Layout.MinY + Layout.MaxY) * 0.5f;

            for (const FLaidOutGlyph& G : Layout.Glyphs)
            {
                const float X0 = G.MinX - CenterX;
                const float YBottom = G.MaxY - CenterY;

                const FVector BottomLeft = TextOrigin + RightAxis * X0 - UpAxis * YBottom;
                const FVector GlyphRight = RightAxis * (G.MaxX - G.MinX);
                const FVector GlyphUp = UpAxis * (G.MaxY - G.MinY);
                const FColor  OutlineColor = G.bSolidColorQuad ? G.SolidColor : Item.Color;

                SubmitQuadOutline(InLineRenderer, BottomLeft, GlyphRight, GlyphUp, OutlineColor);
            }
        }
    }
}
