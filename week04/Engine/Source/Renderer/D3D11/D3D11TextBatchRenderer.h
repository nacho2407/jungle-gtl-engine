#pragma once

#include "Core/HAL/PlatformTypes.h"
#include "Core/Math/Color.h"
#include "Core/Math/Matrix.h"
#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/Types/RenderDebugColors.h"
#include "Renderer/Types/RenderItem.h"
#include "Renderer/Types/ShaderConstants.h"
#include "Renderer/Types/VertexTypes.h"
#include <vector>

class FD3D11RHI;
class FSceneView;
struct FFontResource;
struct FFontGlyph;
struct FTextRenderItem;

class FD3D11TextBatchRenderer
{
  public:
    bool Initialize(FD3D11RHI* InRHI);
    void Shutdown();

    void BeginFrame();
    void BeginFrame(const FSceneView* InSceneView);

    void AddText(const FTextRenderItem& InItem);
    void AddTexts(const TArray<FTextRenderItem>& InItems);
    void EndFrame(const FSceneView* InSceneView);
    void Flush(const FSceneView* InSceneView);

  private:
    enum class EResolvedGlyphKind : uint8
    {
        Normal,
        QuestionFallback,
        Missing
    };

    struct FResolvedGlyph
    {
        const FFontGlyph*  Glyph = nullptr;
        EResolvedGlyphKind Kind = EResolvedGlyphKind::Missing;
    };

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

    struct FTextBatchKey
    {
        const FFontResource* FontResource = nullptr;
        ERenderPlacementMode PlacementMode = ERenderPlacementMode::World;

        bool operator==(const FTextBatchKey& Other) const
        {
            return FontResource == Other.FontResource && PlacementMode == Other.PlacementMode;
        }

        bool operator!=(const FTextBatchKey& Other) const { return !(*this == Other); }
    };

    bool CreateShaders();
    bool CreateConstantBuffer();
    bool CreateStates();
    bool CreateBuffers();

    FResolvedGlyph ResolveGlyph(const FFontResource& InFont, uint32 InCodePoint) const;
    float          GetMissingGlyphAdvance(const FFontResource& InFont, float InLineHeight,
                                          float InScale) const;

    FTextLayout BuildTextLayout(const FTextRenderItem& InItem) const;

    void AppendGlyphQuad(const FVector& InBottomLeft, const FVector& InRight, const FVector& InUp,
                         const FFontGlyph& InGlyph, const FFontResource& InFont,
                         const FColor& InColor);

    void AppendSolidColorQuad(const FVector& InBottomLeft, const FVector& InRight,
                              const FVector& InUp, const FColor& InColor);

    void          BeginBatch(const FTextBatchKey& InBatchKey);
    void          AppendTextItem(const FTextRenderItem& InItem);
    void          AppendTextItemNatural(const FTextRenderItem& InItem);
    void          AppendTextItemFitToBox(const FTextRenderItem& InItem);
    void          ProcessSortedItems();
    FTextBatchKey MakeBatchKey(const FTextRenderItem& InItem) const;
    bool          CanAppendGlyphQuad() const;

  private:
    static constexpr uint32         MaxVertexCount = 65536;
    static constexpr uint32         MaxIndexCount = 65536;
    static constexpr const wchar_t* ShaderPath = L"../Engine/Resources/Shader/ShaderFont.hlsl";

    FD3D11RHI* RHI = nullptr;

    const FSceneView*    CurrentSceneView = nullptr;
    const FFontResource* CurrentFontResource = nullptr;
    ERenderPlacementMode CurrentPlacementMode = ERenderPlacementMode::World;

    TArray<FFontVertex>     Vertices;
    TArray<uint32>          Indices;
    TArray<FTextRenderItem> PendingTextItems;

    TComPtr<ID3D11VertexShader>      VertexShader;
    TComPtr<ID3D11PixelShader>       PixelShader;
    TComPtr<ID3D11InputLayout>       InputLayout;
    TComPtr<ID3D11Buffer>            ConstantBuffer;
    TComPtr<ID3D11Buffer>            DynamicVertexBuffer;
    TComPtr<ID3D11Buffer>            DynamicIndexBuffer;
    TComPtr<ID3D11SamplerState>      SamplerState;
    TComPtr<ID3D11BlendState>        AlphaBlendState;
    TComPtr<ID3D11DepthStencilState> DepthStencilState;
    TComPtr<ID3D11RasterizerState>   RasterizerState;
};
