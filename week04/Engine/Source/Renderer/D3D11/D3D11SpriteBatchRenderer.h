#pragma once

#include "Core/HAL/PlatformTypes.h"
#include "Core/Math/Color.h"
#include "Core/Math/Matrix.h"
#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/Types/RenderItem.h"
#include <vector>

class FD3D11RHI;
class FSceneView;
struct FTextureResource;

class FD3D11SpriteBatchRenderer
{
  public:
    bool Initialize(FD3D11RHI* InRHI);
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

    struct FSpriteBatchKey
    {
        const FTextureResource* TextureResource = nullptr;
        ERenderPlacementMode    PlacementMode = ERenderPlacementMode::World;

        bool operator==(const FSpriteBatchKey& Other) const
        {
            return TextureResource == Other.TextureResource && PlacementMode == Other.PlacementMode;
        }

        bool operator!=(const FSpriteBatchKey& Other) const { return !(*this == Other); }
    };

    bool CreateShaders();
    bool CreateConstantBuffer();
    bool CreateStates();
    bool CreateBuffers();

    void            BeginBatch(const FSpriteBatchKey& InBatchKey);
    void            AppendSpriteItem(const FSpriteRenderItem& InItem);
    void            ProcessSortedItems();
    FSpriteBatchKey MakeBatchKey(const FSpriteRenderItem& InItem) const;
    bool            CanAppendQuad() const;
    void AppendQuad(const FVector& InBottomLeft, const FVector& InRight, const FVector& InUp,
                    const FVector2& InUVMin, const FVector2& InUVMax, const FColor& InColor);

  private:
    static constexpr uint32         MaxVertexCount = 65536;
    static constexpr uint32         MaxIndexCount = 65536;
    static constexpr const wchar_t* ShaderPath = L"Content\\Shader\\ShaderSprite.hlsl";

    FD3D11RHI* RHI = nullptr;

    const FSceneView*       CurrentSceneView = nullptr;
    const FTextureResource* CurrentTextureResource = nullptr;
    ERenderPlacementMode    CurrentPlacementMode = ERenderPlacementMode::World;

    TArray<FSpriteVertex>     Vertices;
    TArray<uint32>            Indices;
    TArray<FSpriteRenderItem> PendingSpriteItems;

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
