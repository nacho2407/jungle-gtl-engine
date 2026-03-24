#pragma once

#include "Core/HAL/PlatformTypes.h"
#include "Core/Math/Color.h"
#include "Core/Math/Matrix.h"
#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/Types/RenderItem.h"
#include "Renderer/Types/VertexTypes.h"
#include "Renderer/Types/ShaderConstants.h"
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
    struct FTextBatchKey
    {
        const FFontResource*  FontResource = nullptr;
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

    void AppendGlyphQuad(const FVector& InBottomLeft, const FVector& InRight, const FVector& InUp,
                         const FFontGlyph& InGlyph, const FFontResource& InFont,
                         const FColor& InColor);

    void          BeginBatch(const FTextBatchKey& InBatchKey);
    void          AppendTextItem(const FTextRenderItem& InItem);
    void          ProcessSortedItems();
    FTextBatchKey MakeBatchKey(const FTextRenderItem& InItem) const;
    bool          CanAppendGlyphQuad() const;

  private:
    static constexpr uint32         MaxVertexCount = 65536;
    static constexpr uint32         MaxIndexCount = 65536;
    static constexpr const wchar_t* ShaderPath = L"Content\\Shader\\ShaderFont.hlsl";

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
