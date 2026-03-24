#pragma once

#include "Core/HAL/PlatformTypes.h"
#include "Core/Math/Color.h"
#include "Core/Math/Matrix.h"
#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/Types/RenderItem.h"
#include <vector>

class FD3D11DynamicRHI;
class FSceneView;
class FTextureResource;

class FD3D11SpriteBatchRenderer
{
  public:
    bool Initialize(FD3D11DynamicRHI* InRHI);
    void Shutdown();

    void BeginFrame();
    void BeginFrame(const FSceneView* InSceneView);

    void AddSprite(const FSpriteRenderItem& InItem);
    void AddSprites(const TArray<FSpriteRenderItem>& InItems);
    void EndFrame(const FSceneView* InSceneView);
    void Flush(const FSceneView* InSceneView);

  private:
    struct FSpriteVertex
    {
        FVector  Position;
        FVector2 UV;
        FColor   Color;
    };

    struct FSpriteConstants
    {
        FMatrix VP;
    };

    bool CreateShaders();
    bool CreateConstantBuffer();
    bool CreateStates();
    bool CreateBuffers();

    void AppendSpriteItem(const FSpriteRenderItem& InItem);
    void AppendQuad(const FVector& InBottomLeft, const FVector& InRight, const FVector& InUp,
                    const FVector2& InUVMin, const FVector2& InUVMax, const FColor& InColor);

    FVector MakeScreenClipPosition(float InScreenX, float InScreenY, float InDepth) const;
    bool    IsSameBatch(const FSpriteRenderItem& InItem) const;

  private:
    static constexpr uint32         MaxVertexCount = 65536;
    static constexpr uint32         MaxIndexCount = 65536;
    static constexpr const wchar_t* ShaderPath = L"Engine/Resources/Shader/ShaderSprite.hlsl";

    FD3D11DynamicRHI* RHI = nullptr;

    const FSceneView*       CurrentSceneView = nullptr;
    const FTextureResource* CurrentTextureResource = nullptr;
    ERenderPlacementMode    CurrentPlacementMode = ERenderPlacementMode::World;

    TArray<FSpriteVertex>     Vertices;
    TArray<uint32>            Indices;
    TArray<FSpriteRenderItem> PendingSpriteItems;
    uint64                    NextSubmissionOrder = 0;

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
