#pragma once

#include "Containers/Array.h"
#include "D3D11/D3D11Common.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"
#include "Renderer/Types/VertexTypes.h"

class FD3D11DynamicRHI;
class FSceneView;

class FD3D11FontBatchRenderer
{
  public:
    void Initialize(FD3D11DynamicRHI *InRHI) {}
    void Shutdown() {}

    void BeginFrame() {}
    void DrawTextScreenSpace(float InX, float InY, const char *InText, float InScale,
                             const FVector4 &InColor) {}
    void DrawTextWorldSpace(const FSceneView *InSceneView, const FVector &InWorldPosition,
                            const char *InText, float InScale, const FVector4 &InColor) {}
    void EndFrame(const FSceneView *InSceneView) {}

  private:
    void CreateShaders() {}
    void CreateInputLayout() {}
    void CreateConstantBuffer() {}
    void CreateStates() {}
    void CreateBuffers() {}
    void CreateFontTexture() {}
    void Flush(const FSceneView *InSceneView);

  private:
    FD3D11DynamicRHI *RHI = nullptr;

    TArray<FFontVertex> Vertices;
    TArray<uint32>      Indices;

    TComPtr<ID3D11VertexShader> VertexShader;
    TComPtr<ID3D11PixelShader>  PixelShader;
    TComPtr<ID3D11InputLayout>  InputLayout;

    TComPtr<ID3D11Buffer> ConstantBuffer;
    TComPtr<ID3D11Buffer> DynamicVertexBuffer;
    TComPtr<ID3D11Buffer> DynamicIndexBuffer;

    TComPtr<ID3D11ShaderResourceView> FontTextureSRV;
    TComPtr<ID3D11SamplerState>       SamplerState;
    TComPtr<ID3D11BlendState>         AlphaBlendState;
};