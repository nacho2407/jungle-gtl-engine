#pragma once

#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/Types/RenderItem.h"
#include "Renderer/Types/ShaderConstants.h"
#include "Renderer/Types/VertexTypes.h"

class FD3D11RHI;
class FSceneView;
struct FTextureResource;
struct FFontResource;
struct FFontGlyph;

class FD3D11SelectionMaskRenderer
{
  public:
    bool Initialize(FD3D11RHI* InRHI);
    void Shutdown();

    bool Resize(int32 InWidth, int32 InHeight);

    void BeginFrame(const FSceneView* InSceneView, bool bInEnableOcclusion);

    void AddStaticMesh(const FStaticMeshRenderItem& InItem);
    void AddStaticMeshes(const TArray<FStaticMeshRenderItem>& InItems);

    void AddSprite(const FSpriteRenderItem& InItem);
    void AddSprites(const TArray<FSpriteRenderItem>& InItems);

    void AddText(const FTextRenderItem& InItem);
    void AddTexts(const TArray<FTextRenderItem>& InItems);

    void EndFrame();

    ID3D11ShaderResourceView* GetMaskSRV() const { return MaskSRV.Get(); }

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

        bool bSolidColorQuad = false;
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

    struct FSpriteBatchKey
    {
        const FTextureResource* TextureResource = nullptr;
        ERenderPlacementMode    PlacementMode = ERenderPlacementMode::World;

        bool operator==(const FSpriteBatchKey& Other) const
        {
            return TextureResource == Other.TextureResource && PlacementMode == Other.PlacementMode;
        }
    };

    struct FTextBatchKey
    {
        const FFontResource* FontResource = nullptr;
        ERenderPlacementMode PlacementMode = ERenderPlacementMode::World;

        bool operator==(const FTextBatchKey& Other) const
        {
            return FontResource == Other.FontResource && PlacementMode == Other.PlacementMode;
        }
    };

    bool CreateMaskResources(int32 InWidth, int32 InHeight);
    void ReleaseMaskResources();

    bool CreateStaticMeshShaders();
    bool CreateSpriteShaders();
    bool CreateTextShaders();
    bool CreateConstantBuffers();
    bool CreateStates();
    bool CreateDynamicBuffers();
    bool CreateFallbackWhiteTexture();

    void DrawStaticMeshes();
    void DrawSprites();
    void DrawTexts();

    void ProcessSortedSprites();
    void ProcessSortedTexts();

    FSpriteBatchKey MakeSpriteBatchKey(const FSpriteRenderItem& InItem) const;
    void BeginSpriteBatch(const FSpriteBatchKey& InBatchKey);
    void AppendSpriteItem(const FSpriteRenderItem& InItem);
    void AppendSpriteQuad(const FVector& InBottomLeft, const FVector& InRight, const FVector& InUp,
                          const FVector2& InUVMin, const FVector2& InUVMax);

    FTextBatchKey MakeTextBatchKey(const FTextRenderItem& InItem) const;
    void BeginTextBatch(const FTextBatchKey& InBatchKey);
    void AppendTextItem(const FTextRenderItem& InItem);
    void AppendTextItemNatural(const FTextRenderItem& InItem);
    void AppendTextItemFitToBox(const FTextRenderItem& InItem);
    void AppendTextNullFontFallback(const FTextRenderItem& InItem);
    void AppendGlyphQuad(const FVector& InBottomLeft, const FVector& InRight, const FVector& InUp,
                         const FFontGlyph& InGlyph, const FFontResource& InFont);
    void AppendSolidQuad(const FVector& InBottomLeft, const FVector& InRight, const FVector& InUp);

    bool CanAppendSpriteQuad() const;
    bool CanAppendTextQuad() const;
    void FlushSpriteBatch();
    void FlushTextBatch();

    ID3D11ShaderResourceView* ResolveSpriteSRV(const FTextureResource* InTextureResource) const;
    ID3D11ShaderResourceView* ResolveFontSRV(const FFontResource* InFontResource) const;

    FResolvedGlyph ResolveGlyph(const FFontResource& InFont, uint32 InCodePoint) const;
    float          GetMissingGlyphAdvance(const FFontResource& InFont, float InLineHeight,
                                          float InScale) const;
    FTextLayout    BuildTextLayout(const FTextRenderItem& InItem) const;

    void BuildPlacementAxes(const FRenderPlacement& InPlacement, FVector& OutOrigin,
                            FVector& OutRightAxis, FVector& OutUpAxis) const;

  private:
    static constexpr uint32 MaxVertexCount = 65536;
    static constexpr uint32 MaxIndexCount = 65536;
    static constexpr const wchar_t* StaticMeshShaderPath =
        L"Content\\Shader\\SelectionMaskMesh.hlsl";
    static constexpr const wchar_t* SpriteShaderPath =
        L"Content\\Shader\\SelectionMaskSprite.hlsl";
    static constexpr const wchar_t* TextShaderPath = L"Content\\Shader\\ShaderFont.hlsl";

    FD3D11RHI*        RHI = nullptr;
    const FSceneView* CurrentSceneView = nullptr;
    bool              bEnableOcclusion = true;

    TArray<FStaticMeshRenderItem> PendingStaticMeshes;
    TArray<FSpriteRenderItem>     PendingSprites;
    TArray<FTextRenderItem>       PendingTexts;

    TArray<FSpriteVertex> SpriteVertices;
    TArray<uint32>        SpriteIndices;
    TArray<FFontVertex>   TextVertices;
    TArray<uint32>        TextIndices;

    const FTextureResource* CurrentSpriteTexture = nullptr;
    ERenderPlacementMode    CurrentSpritePlacementMode = ERenderPlacementMode::World;
    const FFontResource*    CurrentFontResource = nullptr;
    ERenderPlacementMode    CurrentTextPlacementMode = ERenderPlacementMode::World;

    TComPtr<ID3D11VertexShader> StaticMeshVertexShader;
    TComPtr<ID3D11PixelShader>  StaticMeshPixelShader;
    TComPtr<ID3D11InputLayout>  StaticMeshInputLayout;

    TComPtr<ID3D11VertexShader> SpriteVertexShader;
    TComPtr<ID3D11PixelShader>  SpritePixelShader;
    TComPtr<ID3D11InputLayout>  SpriteInputLayout;

    TComPtr<ID3D11VertexShader> TextVertexShader;
    TComPtr<ID3D11PixelShader>  TextPixelShader;
    TComPtr<ID3D11InputLayout>  TextInputLayout;

    TComPtr<ID3D11Buffer> MeshConstantBuffer;
    TComPtr<ID3D11Buffer> SpriteConstantBuffer;
    TComPtr<ID3D11Buffer> TextConstantBuffer;

    TComPtr<ID3D11Buffer> DynamicSpriteVertexBuffer;
    TComPtr<ID3D11Buffer> DynamicSpriteIndexBuffer;
    TComPtr<ID3D11Buffer> DynamicTextVertexBuffer;
    TComPtr<ID3D11Buffer> DynamicTextIndexBuffer;

    TComPtr<ID3D11SamplerState>      SamplerState;
    TComPtr<ID3D11BlendState>        AlphaBlendState;
    TComPtr<ID3D11DepthStencilState> DepthEnabledState;
    TComPtr<ID3D11DepthStencilState> DepthDisabledState;
    TComPtr<ID3D11RasterizerState>   RasterizerState;

    TComPtr<ID3D11Texture2D>          FallbackWhiteTexture;
    TComPtr<ID3D11ShaderResourceView> FallbackWhiteSRV;

    TComPtr<ID3D11Texture2D>          MaskTexture;
    TComPtr<ID3D11RenderTargetView>   MaskRTV;
    TComPtr<ID3D11ShaderResourceView> MaskSRV;
    TComPtr<ID3D11Texture2D>          MaskDepthTexture;
    TComPtr<ID3D11DepthStencilView>   MaskDSV;
    TComPtr<ID3D11DepthStencilView>   ActiveDepthTarget;

    uint64 MaskRenderTargetBytes = 0;
    uint64 MaskDepthBytes = 0;
};
