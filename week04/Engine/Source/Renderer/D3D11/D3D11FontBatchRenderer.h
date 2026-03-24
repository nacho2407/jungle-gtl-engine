#pragma once

#include "Core/HAL/PlatformTypes.h"
#include "Core/Math/Color.h"
#include "Core/Math/Matrix.h"
#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/Types/RenderItem.h"
#include "Renderer/Types/VertexTypes.h"
#include "Renderer/Types/ShaderConstants.h"
#include <vector>

class FD3D11DynamicRHI;
class FSceneView;
class FFontResource;
struct FFontGlyph;
struct FTextRenderItem;

class FD3D11FontBatchRenderer
{
  public:
    bool Initialize(FD3D11DynamicRHI* InRHI);
    void Shutdown();

    void BeginFrame();
    void BeginFrame(const FSceneView* InSceneView);

    void AddText(const FTextRenderItem& InItem);
    void EndFrame(const FSceneView* InSceneView);
    void Flush(const FSceneView* InSceneView);

  private:
    bool CreateShaders();
    bool CreateConstantBuffer();
    bool CreateStates();
    bool CreateBuffers();

    void AppendGlyphQuad(const FVector& InBottomLeft, const FVector& InRight, const FVector& InUp,
                         const FFontGlyph& InGlyph, const FFontResource& InFont,
                         const FColor& InColor);

    void    AppendTextItem(const FTextRenderItem& InItem);
    FVector MakeScreenClipPosition(float InScreenX, float InScreenY, float InDepth) const;
    bool    IsSameBatch(const FTextRenderItem& InItem) const;

  private:
    static constexpr uint32         MaxVertexCount = 65536;
    static constexpr uint32         MaxIndexCount = 65536;
    static constexpr const wchar_t* ShaderPath = L"../Engine/Resources/Shader/ShaderFont.hlsl";

    FD3D11DynamicRHI* RHI = nullptr;

    const FSceneView*    CurrentSceneView = nullptr;
    const FFontResource* CurrentFontResource = nullptr;
    ERenderPlacementMode CurrentPlacementMode = ERenderPlacementMode::World;

    TArray<FFontVertex>     Vertices;
    TArray<uint32>          Indices;
    TArray<FTextRenderItem> PendingTextItems;
    uint64                  NextSubmissionOrder = 0;

    TComPtr<ID3D11VertexShader>      VertexShader;
    TComPtr<ID3D11PixelShader>       PixelShader;
    TComPtr<ID3D11InputLayout>       InputLayout;
    TComPtr<ID3D11Buffer>            ConstantBuffer;
    TComPtr<ID3D11Buffer>            DynamicVertexBuffer;
    TComPtr<ID3D11Buffer>            DynamicIndexBuffer;
    TComPtr<ID3D11SamplerState>      SamplerState;
    TComPtr<ID3D11BlendState>        AlphaBlendState;
    TComPtr<ID3D11DepthStencilState> DepthStencilState;
    TComPtr<ID3D11DepthStencilState> ScreenDepthStencilState;
    TComPtr<ID3D11RasterizerState>   RasterizerState;
};
