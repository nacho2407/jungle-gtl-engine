#include "Renderer/D3D11/D3D11TextBatchRenderer.h"

#include "Renderer/D3D11/D3D11RHI.h"
#include "Renderer/RenderAsset/FontResource.h"
#include "Renderer/SceneView.h"

#include <algorithm>
#include <functional>

namespace
{
    constexpr float DefaultLineHeight = 16.0f;
    constexpr uint32 Utf8ReplacementCodePoint = static_cast<uint32>('?');

    TArray<uint32> DecodeUtf8CodePoints(const FString& InText)
    {
        TArray<uint32> CodePoints;
        CodePoints.reserve(InText.size());

        const uint8* Bytes = reinterpret_cast<const uint8*>(InText.data());
        const size_t ByteCount = InText.size();

        size_t Index = 0;
        while (Index < ByteCount)
        {
            const uint8 Lead = Bytes[Index];

            if (Lead <= 0x7F)
            {
                CodePoints.push_back(static_cast<uint32>(Lead));
                ++Index;
                continue;
            }

            uint32 CodePoint = Utf8ReplacementCodePoint;
            size_t SequenceLength = 1;

            auto IsContinuationByte =
                [Bytes, ByteCount](size_t InIndex)
            {
                return InIndex < ByteCount && (Bytes[InIndex] & 0xC0) == 0x80;
            };

            if ((Lead & 0xE0) == 0xC0)
            {
                SequenceLength = 2;
                if (IsContinuationByte(Index + 1))
                {
                    const uint32 Candidate =
                        ((static_cast<uint32>(Lead & 0x1F)) << 6) |
                        static_cast<uint32>(Bytes[Index + 1] & 0x3F);

                    if (Candidate >= 0x80)
                    {
                        CodePoint = Candidate;
                    }
                }
            }
            else if ((Lead & 0xF0) == 0xE0)
            {
                SequenceLength = 3;
                if (IsContinuationByte(Index + 1) && IsContinuationByte(Index + 2))
                {
                    const uint32 Candidate =
                        ((static_cast<uint32>(Lead & 0x0F)) << 12) |
                        ((static_cast<uint32>(Bytes[Index + 1] & 0x3F)) << 6) |
                        static_cast<uint32>(Bytes[Index + 2] & 0x3F);

                    if (Candidate >= 0x800 && !(Candidate >= 0xD800 && Candidate <= 0xDFFF))
                    {
                        CodePoint = Candidate;
                    }
                }
            }
            else if ((Lead & 0xF8) == 0xF0)
            {
                SequenceLength = 4;
                if (IsContinuationByte(Index + 1) && IsContinuationByte(Index + 2) &&
                    IsContinuationByte(Index + 3))
                {
                    const uint32 Candidate =
                        ((static_cast<uint32>(Lead & 0x07)) << 18) |
                        ((static_cast<uint32>(Bytes[Index + 1] & 0x3F)) << 12) |
                        ((static_cast<uint32>(Bytes[Index + 2] & 0x3F)) << 6) |
                        static_cast<uint32>(Bytes[Index + 3] & 0x3F);

                    if (Candidate >= 0x10000 && Candidate <= 0x10FFFF)
                    {
                        CodePoint = Candidate;
                    }
                }
            }

            CodePoints.push_back(CodePoint);
            Index += SequenceLength;
        }

        return CodePoints;
    }

    float ComputePlacementDepth(const FSceneView* InSceneView, const FRenderPlacement& InPlacement)
    {
        if (InSceneView == nullptr)
        {
            return 0.0f;
        }

        const FMatrix CameraWorld = InSceneView->GetViewMatrix().GetInverse();
        const FVector CameraOrigin = CameraWorld.GetOrigin();
        const FVector CameraForward = CameraWorld.GetForwardVector();
        const FVector WorldOrigin = InPlacement.World.GetOrigin() + InPlacement.WorldOffset;
        const FVector Delta = WorldOrigin - CameraOrigin;
        return Delta.X * CameraForward.X + Delta.Y * CameraForward.Y + Delta.Z * CameraForward.Z;
    }

    struct FTextRenderItemLess
    {
        const FSceneView* SceneView = nullptr;

        bool operator()(const FTextRenderItem& A, const FTextRenderItem& B) const
        {
            const float DepthA = ComputePlacementDepth(SceneView, A.Placement);
            const float DepthB = ComputePlacementDepth(SceneView, B.Placement);

            if (DepthA != DepthB)
            {
                return DepthA > DepthB;
            }

            if (A.Placement.Mode != B.Placement.Mode)
            {
                return static_cast<uint8>(A.Placement.Mode) < static_cast<uint8>(B.Placement.Mode);
            }

            return std::less<const FFontResource*>{}(A.FontResource, B.FontResource);
        }
    };
} // namespace

bool FD3D11TextBatchRenderer::Initialize(FD3D11RHI* InRHI)
{
    if (InRHI == nullptr)
    {
        return false;
    }

    RHI = InRHI;
    CurrentSceneView = nullptr;
    CurrentFontResource = nullptr;
    CurrentPlacementMode = ERenderPlacementMode::World;

    Vertices.reserve(MaxVertexCount);
    Indices.reserve(MaxIndexCount);
    PendingTextItems.reserve(1024);

    if (!CreateShaders())
    {
        Shutdown();
        return false;
    }

    if (!CreateConstantBuffer())
    {
        Shutdown();
        return false;
    }

    if (!CreateStates())
    {
        Shutdown();
        return false;
    }

    if (!CreateBuffers())
    {
        Shutdown();
        return false;
    }

    if (!CreateFallbackWhiteTexture())
    {
        Shutdown();
        return false;
    }

    return true;
}

void FD3D11TextBatchRenderer::Shutdown()
{
    RasterizerState.Reset();
    DepthStencilState.Reset();
    AlphaBlendState.Reset();
    SamplerState.Reset();
    FallbackWhiteSRV.Reset();
    FallbackWhiteTexture.Reset();

    DynamicIndexBuffer.Reset();
    DynamicVertexBuffer.Reset();
    ConstantBuffer.Reset();

    InputLayout.Reset();
    PixelShader.Reset();
    VertexShader.Reset();

    Vertices.clear();
    Indices.clear();
    PendingTextItems.clear();

    CurrentFontResource = nullptr;
    CurrentSceneView = nullptr;
    CurrentPlacementMode = ERenderPlacementMode::World;
    RHI = nullptr;
}

void FD3D11TextBatchRenderer::BeginFrame() { BeginFrame(CurrentSceneView); }

void FD3D11TextBatchRenderer::BeginFrame(const FSceneView* InSceneView)
{
    CurrentSceneView = InSceneView;
    CurrentFontResource = nullptr;
    CurrentPlacementMode = ERenderPlacementMode::World;

    Vertices.clear();
    Indices.clear();
    PendingTextItems.clear();
}

void FD3D11TextBatchRenderer::AddText(const FTextRenderItem& InItem)
{
    if (RHI == nullptr || CurrentSceneView == nullptr)
    {
        return;
    }

    if (!InItem.State.IsVisible() || InItem.Text.empty())
    {
        return;
    }

    PendingTextItems.push_back(InItem);
}

void FD3D11TextBatchRenderer::AddTexts(const TArray<FTextRenderItem>& InItems)
{
    for (const FTextRenderItem& Item : InItems)
    {
        AddText(Item);
    }
}

FD3D11TextBatchRenderer::FResolvedGlyph
FD3D11TextBatchRenderer::ResolveGlyph(const FFontResource& InFont, uint32 InCodePoint) const
{
    if (const FFontGlyph* Glyph = InFont.FindGlyph(InCodePoint))
    {
        if (Glyph->IsValid())
        {
            return {Glyph, EResolvedGlyphKind::Normal};
        }
    }

    if (const FFontGlyph* QuestionGlyph = InFont.FindGlyph(static_cast<uint32>('?')))
    {
        if (QuestionGlyph->IsValid())
        {
            return {QuestionGlyph, EResolvedGlyphKind::QuestionFallback};
        }
    }

    return {nullptr, EResolvedGlyphKind::Missing};
}

float FD3D11TextBatchRenderer::GetMissingGlyphAdvance(const FFontResource& InFont,
                                                      float InLineHeight, float InScale) const
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

FD3D11TextBatchRenderer::FTextLayout
FD3D11TextBatchRenderer::BuildTextLayout(const FTextRenderItem& InItem) const
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

    const float MissingAdvance = GetMissingGlyphAdvance(*InItem.FontResource, RawLineHeight, Scale);
    const float MissingWidth = MissingAdvance;
    const float MissingHeight = RawLineHeight * Scale;

    float PenX = 0.0f;
    float PenY = 0.0f;
    bool  bHasBounds = false;

    const TArray<uint32> CodePoints = DecodeUtf8CodePoints(InItem.Text);

    for (uint32 CodePoint : CodePoints)
    {
        if (CodePoint == static_cast<uint32>('\r'))
        {
            continue;
        }

        if (CodePoint == static_cast<uint32>('\n'))
        {
            PenX = 0.0f;
            PenY += RawLineHeight * Scale + LineSpacing;
            continue;
        }

        if (CodePoint == static_cast<uint32>(' '))
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
                SpaceAdvance = RawLineHeight * 0.25f * Scale; // 기존 0.5보다 더 보수적으로
            }

            PenX += SpaceAdvance; // 공백에는 LetterSpacing 미적용
            continue;
        }

        const FResolvedGlyph Resolved = ResolveGlyph(*InItem.FontResource, CodePoint);

        FLaidOutGlyph OutGlyph;

        if (Resolved.Kind == EResolvedGlyphKind::Missing)
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

FD3D11TextBatchRenderer::FTextBatchKey
FD3D11TextBatchRenderer::MakeBatchKey(const FTextRenderItem& InItem) const
{
    FTextBatchKey BatchKey = {};
    BatchKey.FontResource = InItem.FontResource;
    BatchKey.PlacementMode = InItem.Placement.Mode;
    return BatchKey;
}

bool FD3D11TextBatchRenderer::CanAppendGlyphQuad() const
{
    return Vertices.size() + 4 <= MaxVertexCount && Indices.size() + 6 <= MaxIndexCount;
}

void FD3D11TextBatchRenderer::BeginBatch(const FTextBatchKey& InBatchKey)
{
    CurrentFontResource = InBatchKey.FontResource;
    CurrentPlacementMode = InBatchKey.PlacementMode;
}

void FD3D11TextBatchRenderer::AppendTextItem(const FTextRenderItem& InItem)
{
    if (InItem.FontResource == nullptr)
    {
        AppendNullFontFallback(InItem);
        return;
    }

    switch (InItem.LayoutMode)
    {
    case ETextLayoutMode::FitToBox:
        AppendTextItemFitToBox(InItem);
        break;

    case ETextLayoutMode::Natural:
    default:
        AppendTextItemNatural(InItem);
        break;
    }
}

void FD3D11TextBatchRenderer::AppendNullFontFallback(const FTextRenderItem& InItem)
{
    if (CurrentSceneView == nullptr)
    {
        return;
    }

    if (!CanAppendGlyphQuad())
    {
        Flush(CurrentSceneView);
        BeginBatch(MakeBatchKey(InItem));
    }

    const FMatrix& PlacementWorld = InItem.Placement.World;
    const FVector  Origin = PlacementWorld.GetOrigin() + InItem.Placement.WorldOffset;

    FVector RightAxis;
    FVector UpAxis;

    if (InItem.Placement.IsBillboard())
    {
        const FMatrix CameraWorld = CurrentSceneView->GetViewMatrix().GetInverse();
        RightAxis = CameraWorld.GetRightVector().GetSafeNormal();
        UpAxis = CameraWorld.GetUpVector().GetSafeNormal();
    }
    else
    {
        RightAxis = PlacementWorld.GetRightVector().GetSafeNormal();
        UpAxis = PlacementWorld.GetUpVector().GetSafeNormal();
    }

    float Width = 1.0f;
    float Height = 1.0f;

    if (InItem.LayoutMode == ETextLayoutMode::FitToBox)
    {
        const FVector WorldScale = PlacementWorld.GetScaleVector();
        Width = std::max(WorldScale.X, 1.0f);
        Height = std::max(WorldScale.Y, 1.0f);
    }
    else
    {
        const float FallbackExtent = std::max(InItem.TextScale, 1.0f);
        Width = FallbackExtent;
        Height = FallbackExtent;
    }

    const FVector QuadRight = RightAxis * Width;
    const FVector QuadUp = UpAxis * Height;
    const FVector BottomLeft = Origin - QuadRight * 0.5f - QuadUp * 0.5f;

    AppendSolidColorQuad(BottomLeft, QuadRight, -QuadUp, RenderDebugColors::MissingGlyph);
}

void FD3D11TextBatchRenderer::AppendTextItemNatural(const FTextRenderItem& InItem)
{
    if (CurrentSceneView == nullptr || InItem.FontResource == nullptr)
    {
        return;
    }

    const FTextLayout Layout = BuildTextLayout(InItem);
    if (!Layout.IsValid())
    {
        return;
    }

    const FMatrix& PlacementWorld = InItem.Placement.World;
    const FVector  TextOrigin = PlacementWorld.GetOrigin() + InItem.Placement.WorldOffset;

    FVector RightAxis;
    FVector UpAxis;

    if (InItem.Placement.IsBillboard())
    {
        const FMatrix CameraWorld = CurrentSceneView->GetViewMatrix().GetInverse();
        RightAxis = CameraWorld.GetRightVector().GetSafeNormal();
        UpAxis = CameraWorld.GetUpVector().GetSafeNormal();
    }
    else
    {
        RightAxis = PlacementWorld.GetRightVector().GetSafeNormal();
        UpAxis = PlacementWorld.GetUpVector().GetSafeNormal();
    }

    const float CenterX = (Layout.MinX + Layout.MaxX) * 0.5f;
    const float CenterY = (Layout.MinY + Layout.MaxY) * 0.5f;

    for (const FLaidOutGlyph& G : Layout.Glyphs)
    {
        if (!CanAppendGlyphQuad())
        {
            Flush(CurrentSceneView);
            BeginBatch(MakeBatchKey(InItem));
        }

        const float X0 = G.MinX - CenterX;
        const float YBottom = G.MaxY - CenterY;

        const FVector BottomLeft = TextOrigin + RightAxis * X0 - UpAxis * YBottom;
        const FVector GlyphRight = RightAxis * (G.MaxX - G.MinX);
        const FVector GlyphUp = UpAxis * (G.MaxY - G.MinY);

        if (G.bSolidColorQuad)
        {
            AppendSolidColorQuad(BottomLeft, GlyphRight, GlyphUp, G.SolidColor);
        }
        else if (G.Glyph != nullptr)
        {
            AppendGlyphQuad(BottomLeft, GlyphRight, GlyphUp, *G.Glyph, *InItem.FontResource,
                            InItem.Color);
        }
    }
}

void FD3D11TextBatchRenderer::AppendTextItemFitToBox(const FTextRenderItem& InItem)
{
    if (CurrentSceneView == nullptr || InItem.FontResource == nullptr)
    {
        return;
    }

    const FTextLayout Layout = BuildTextLayout(InItem);
    if (!Layout.IsValid())
    {
        return;
    }

    const FMatrix& PlacementWorld = InItem.Placement.World;
    const FVector  TextOrigin = PlacementWorld.GetOrigin() + InItem.Placement.WorldOffset;
    const FVector  WorldScale = PlacementWorld.GetScaleVector();

    const float BoxWidth = std::max(WorldScale.X, 1.0f);
    const float BoxHeight = std::max(WorldScale.Y, 1.0f);

    const float LayoutWidth = Layout.GetWidth();
    const float LayoutHeight = Layout.GetHeight();

    const float UniformScale = std::min(BoxWidth / LayoutWidth, BoxHeight / LayoutHeight);

    const float FinalWidth = LayoutWidth * UniformScale;
    const float FinalHeight = LayoutHeight * UniformScale;

    const float OffsetX = (BoxWidth - FinalWidth) * 0.5f;
    const float OffsetY = (BoxHeight - FinalHeight) * 0.5f;

    FVector RightAxis;
    FVector UpAxis;

    if (InItem.Placement.IsBillboard())
    {
        const FMatrix CameraWorld = CurrentSceneView->GetViewMatrix().GetInverse();
        RightAxis = CameraWorld.GetRightVector().GetSafeNormal();
        UpAxis = CameraWorld.GetUpVector().GetSafeNormal();
    }
    else
    {
        RightAxis = PlacementWorld.GetRightVector().GetSafeNormal();
        UpAxis = PlacementWorld.GetUpVector().GetSafeNormal();
    }

    for (const FLaidOutGlyph& G : Layout.Glyphs)
    {
        if (!CanAppendGlyphQuad())
        {
            Flush(CurrentSceneView);
            BeginBatch(MakeBatchKey(InItem));
        }

        const float X0 = OffsetX + (G.MinX - Layout.MinX) * UniformScale;
        const float Y0 = OffsetY + (G.MinY - Layout.MinY) * UniformScale;

        const float W = (G.MaxX - G.MinX) * UniformScale;
        const float H = (G.MaxY - G.MinY) * UniformScale;

        const FVector BottomLeft = TextOrigin + RightAxis * X0 - UpAxis * Y0;
        const FVector GlyphRight = RightAxis * W;
        const FVector GlyphUp = UpAxis * H;

        if (G.bSolidColorQuad)
        {
            AppendSolidColorQuad(BottomLeft, GlyphRight, GlyphUp, G.SolidColor);
        }
        else if (G.Glyph != nullptr)
        {
            AppendGlyphQuad(BottomLeft, GlyphRight, GlyphUp, *G.Glyph, *InItem.FontResource,
                            InItem.Color);
        }
    }
}

void FD3D11TextBatchRenderer::ProcessSortedItems()
{
    if (PendingTextItems.empty())
    {
        return;
    }

    FTextBatchKey ActiveBatchKey = {};
    bool          bHasActiveBatch = false;

    for (const FTextRenderItem& Item : PendingTextItems)
    {
        const FTextBatchKey ItemBatchKey = MakeBatchKey(Item);

        if (!bHasActiveBatch)
        {
            BeginBatch(ItemBatchKey);
            bHasActiveBatch = true;
            ActiveBatchKey = ItemBatchKey;
        }
        else if (ItemBatchKey != ActiveBatchKey)
        {
            Flush(CurrentSceneView);
            BeginBatch(ItemBatchKey);
            ActiveBatchKey = ItemBatchKey;
        }

        AppendTextItem(Item);
    }

    Flush(CurrentSceneView);
}

void FD3D11TextBatchRenderer::EndFrame(const FSceneView* InSceneView)
{
    if (InSceneView == nullptr)
    {
        PendingTextItems.clear();
        Vertices.clear();
        Indices.clear();
        CurrentSceneView = nullptr;
        CurrentFontResource = nullptr;
        CurrentPlacementMode = ERenderPlacementMode::World;
        return;
    }

    CurrentSceneView = InSceneView;
    CurrentFontResource = nullptr;
    CurrentPlacementMode = ERenderPlacementMode::World;
    Vertices.clear();
    Indices.clear();

    std::sort(PendingTextItems.begin(), PendingTextItems.end(),
              FTextRenderItemLess{CurrentSceneView});
    ProcessSortedItems();

    PendingTextItems.clear();
    CurrentSceneView = nullptr;
    CurrentFontResource = nullptr;
    CurrentPlacementMode = ERenderPlacementMode::World;
}

void FD3D11TextBatchRenderer::Flush(const FSceneView* InSceneView)
{
    if (RHI == nullptr || InSceneView == nullptr || Vertices.empty() || Indices.empty())
    {
        Vertices.clear();
        Indices.clear();
        return;
    }

    if (!RHI->UpdateDynamicBuffer(DynamicVertexBuffer.Get(), Vertices.data(),
                                  static_cast<uint32>(Vertices.size() * sizeof(FFontVertex))) ||
        !RHI->UpdateDynamicBuffer(DynamicIndexBuffer.Get(), Indices.data(),
                                  static_cast<uint32>(Indices.size() * sizeof(uint32))))
    {
        Vertices.clear();
        Indices.clear();
        return;
    }

    FFontConstants Constants = {};
    Constants.VP = InSceneView->GetViewProjectionMatrix();
    Constants.TintColor = FColor::White();

    if (!RHI->UpdateConstantBuffer(ConstantBuffer.Get(), &Constants, sizeof(Constants)))
    {
        Vertices.clear();
        Indices.clear();
        return;
    }

    const uint32 VertexStride = sizeof(FFontVertex);
    const uint32 VertexOffset = 0;
    const float  BlendFactor[4] = {0.f, 0.f, 0.f, 0.f};

    RHI->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    RHI->SetInputLayout(InputLayout.Get());
    RHI->SetVertexShader(VertexShader.Get());
    RHI->SetPixelShader(PixelShader.Get());
    RHI->SetVSConstantBuffer(0, ConstantBuffer.Get());
    RHI->SetPSConstantBuffer(0, ConstantBuffer.Get());
    RHI->SetVertexBuffer(0, DynamicVertexBuffer.Get(), VertexStride, VertexOffset);
    RHI->SetIndexBuffer(DynamicIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    RHI->SetRasterizerState(RasterizerState.Get());
    RHI->SetDepthStencilState(DepthStencilState.Get(), 0);
    RHI->SetBlendState(AlphaBlendState.Get(), BlendFactor, 0xFFFFFFFFu);
    RHI->SetPSShaderResource(0, ResolveFontSRV(CurrentFontResource));
    RHI->SetPSSampler(0, SamplerState.Get());

    RHI->DrawIndexed(static_cast<uint32>(Indices.size()), 0, 0);

    RHI->ClearPSShaderResource(0);
    RHI->ClearBlendState();

    Vertices.clear();
    Indices.clear();
}

void FD3D11TextBatchRenderer::AppendGlyphQuad(const FVector& InBottomLeft, const FVector& InRight,
                                              const FVector& InUp, const FFontGlyph& InGlyph,
                                              const FFontResource& InFont, const FColor& InColor)
{
    const float InvW = InFont.GetInvAtlasWidth();
    const float InvH = InFont.GetInvAtlasHeight();

    const float U0 = static_cast<float>(InGlyph.X) * InvW;
    const float V0 = static_cast<float>(InGlyph.Y) * InvH;
    const float U1 = static_cast<float>(InGlyph.X + InGlyph.Width) * InvW;
    const float V1 = static_cast<float>(InGlyph.Y + InGlyph.Height) * InvH;

    const uint32 BaseVertex = static_cast<uint32>(Vertices.size());

    FFontVertex Vtx0;
    Vtx0.Position = InBottomLeft;
    Vtx0.UV = FVector2(U0, V1);
    Vtx0.Color = InColor;

    FFontVertex Vtx1;
    Vtx1.Position = InBottomLeft + InRight;
    Vtx1.UV = FVector2(U1, V1);
    Vtx1.Color = InColor;

    FFontVertex Vtx2;
    Vtx2.Position = InBottomLeft + InUp;
    Vtx2.UV = FVector2(U0, V0);
    Vtx2.Color = InColor;

    FFontVertex Vtx3;
    Vtx3.Position = InBottomLeft + InRight + InUp;
    Vtx3.UV = FVector2(U1, V0);
    Vtx3.Color = InColor;

    Vertices.push_back(Vtx0);
    Vertices.push_back(Vtx1);
    Vertices.push_back(Vtx2);
    Vertices.push_back(Vtx3);

    Indices.push_back(BaseVertex + 0);
    Indices.push_back(BaseVertex + 1);
    Indices.push_back(BaseVertex + 2);

    Indices.push_back(BaseVertex + 2);
    Indices.push_back(BaseVertex + 1);
    Indices.push_back(BaseVertex + 3);
}

void FD3D11TextBatchRenderer::AppendSolidColorQuad(const FVector& InBottomLeft,
                                                   const FVector& InRight, const FVector& InUp,
                                                   const FColor& InColor)
{
    const uint32 BaseVertex = static_cast<uint32>(Vertices.size());

    FFontVertex Vtx0;
    Vtx0.Position = InBottomLeft;
    Vtx0.UV = FVector2(0.0f, 0.0f);
    Vtx0.Color = InColor;

    FFontVertex Vtx1;
    Vtx1.Position = InBottomLeft + InRight;
    Vtx1.UV = FVector2(0.0f, 0.0f);
    Vtx1.Color = InColor;

    FFontVertex Vtx2;
    Vtx2.Position = InBottomLeft - InUp;
    Vtx2.UV = FVector2(0.0f, 0.0f);
    Vtx2.Color = InColor;

    FFontVertex Vtx3;
    Vtx3.Position = InBottomLeft + InRight - InUp;
    Vtx3.UV = FVector2(0.0f, 0.0f);
    Vtx3.Color = InColor;

    Vertices.push_back(Vtx0);
    Vertices.push_back(Vtx1);
    Vertices.push_back(Vtx2);
    Vertices.push_back(Vtx3);

    Indices.push_back(BaseVertex + 0);
    Indices.push_back(BaseVertex + 1);
    Indices.push_back(BaseVertex + 2);

    Indices.push_back(BaseVertex + 2);
    Indices.push_back(BaseVertex + 1);
    Indices.push_back(BaseVertex + 3);
}



ID3D11ShaderResourceView*
FD3D11TextBatchRenderer::ResolveFontSRV(const FFontResource* InFontResource) const
{
    if (InFontResource != nullptr && InFontResource->GetSRV() != nullptr)
    {
        return InFontResource->GetSRV();
    }

    return FallbackWhiteSRV.Get();
}

bool FD3D11TextBatchRenderer::CreateFallbackWhiteTexture()
{
    if (RHI == nullptr || RHI->GetDevice() == nullptr)
    {
        return false;
    }

    ID3D11Device* Device = RHI->GetDevice();

    const uint32 WhitePixel = 0xFFFFFFFFu;

    D3D11_TEXTURE2D_DESC Desc = {};
    Desc.Width = 1;
    Desc.Height = 1;
    Desc.MipLevels = 1;
    Desc.ArraySize = 1;
    Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    Desc.SampleDesc.Count = 1;
    Desc.Usage = D3D11_USAGE_IMMUTABLE;
    Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA InitData = {};
    InitData.pSysMem = &WhitePixel;
    InitData.SysMemPitch = sizeof(uint32);

    if (FAILED(Device->CreateTexture2D(&Desc, &InitData, FallbackWhiteTexture.GetAddressOf())))
    {
        return false;
    }

    return SUCCEEDED(Device->CreateShaderResourceView(FallbackWhiteTexture.Get(), nullptr,
                                                      FallbackWhiteSRV.GetAddressOf()));
}
bool FD3D11TextBatchRenderer::CreateShaders()
{
    if (RHI == nullptr)
    {
        return false;
    }

    static const D3D11_INPUT_ELEMENT_DESC InputElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         static_cast<UINT>(offsetof(FFontVertex, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(FFontVertex, UV)),
         D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         static_cast<UINT>(offsetof(FFontVertex, Color)), D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    if (!RHI->CreateVertexShaderAndInputLayout(
            ShaderPath, "VSMain", InputElements, static_cast<uint32>(std::size(InputElements)),
            VertexShader.GetAddressOf(), InputLayout.GetAddressOf()))
    {
        return false;
    }

    if (!RHI->CreatePixelShader(ShaderPath, "PSMain", PixelShader.GetAddressOf()))
    {
        InputLayout.Reset();
        VertexShader.Reset();
        return false;
    }

    return true;
}

bool FD3D11TextBatchRenderer::CreateConstantBuffer()
{
    return (RHI != nullptr) &&
           RHI->CreateConstantBuffer(sizeof(FFontConstants), ConstantBuffer.GetAddressOf());
}

bool FD3D11TextBatchRenderer::CreateStates()
{
    if (RHI == nullptr)
    {
        return false;
    }

    D3D11_SAMPLER_DESC SamplerDesc = {};
    SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SamplerDesc.MinLOD = 0.0f;
    SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    if (!RHI->CreateSamplerState(SamplerDesc, SamplerState.GetAddressOf()))
    {
        return false;
    }

    D3D11_BLEND_DESC BlendDesc = {};
    BlendDesc.RenderTarget[0].BlendEnable = TRUE;
    BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    if (!RHI->CreateBlendState(BlendDesc, AlphaBlendState.GetAddressOf()))
    {
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC DepthDesc = {};
    DepthDesc.DepthEnable = FALSE;
    DepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    DepthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

    if (!RHI->CreateDepthStencilState(DepthDesc, DepthStencilState.GetAddressOf()))
    {
        return false;
    }

    D3D11_RASTERIZER_DESC RasterizerDesc = {};
    RasterizerDesc.FillMode = D3D11_FILL_SOLID;
    RasterizerDesc.CullMode = D3D11_CULL_NONE;
    RasterizerDesc.DepthClipEnable = TRUE;
    RasterizerDesc.ScissorEnable = FALSE;
    RasterizerDesc.MultisampleEnable = FALSE;
    RasterizerDesc.AntialiasedLineEnable = FALSE;

    return RHI->CreateRasterizerState(RasterizerDesc, RasterizerState.GetAddressOf());
}

bool FD3D11TextBatchRenderer::CreateBuffers()
{
    if (RHI == nullptr || RHI->GetDevice() == nullptr)
    {
        return false;
    }

    D3D11_BUFFER_DESC VertexBufferDesc = {};
    VertexBufferDesc.ByteWidth = sizeof(FFontVertex) * MaxVertexCount;
    VertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    VertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(RHI->GetDevice()->CreateBuffer(&VertexBufferDesc, nullptr,
                                              DynamicVertexBuffer.GetAddressOf())))
    {
        return false;
    }

    D3D11_BUFFER_DESC IndexBufferDesc = {};
    IndexBufferDesc.ByteWidth = sizeof(uint32) * MaxIndexCount;
    IndexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    IndexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    return SUCCEEDED(RHI->GetDevice()->CreateBuffer(&IndexBufferDesc, nullptr,
                                                    DynamicIndexBuffer.GetAddressOf()));
}
