#pragma once

#include "Core/Math/Color.h"
#include "Renderer/D3D11/D3D11Common.h"

class FD3D11RHI;
class FSceneView;

class FD3D11PostProcessOutlineRenderer
{
  public:
    bool Initialize(FD3D11RHI* InRHI);
    void Shutdown();

    void BeginFrame(const FSceneView* InSceneView, ID3D11ShaderResourceView* InMaskSRV);
    void EndFrame();

  private:
    bool CreateShaders();
    bool CreateConstantBuffer();
    bool CreateStates();

  private:
    static constexpr const wchar_t* ShaderPath =
        L"Content\\Shader\\SelectionOutlinePostProcess.hlsl";

    FD3D11RHI*        RHI = nullptr;
    const FSceneView* CurrentSceneView = nullptr;
    ID3D11ShaderResourceView* CurrentMaskSRV = nullptr;

    TComPtr<ID3D11VertexShader> VertexShader;
    TComPtr<ID3D11PixelShader>  PixelShader;
    TComPtr<ID3D11Buffer>       ConstantBuffer;
    TComPtr<ID3D11SamplerState> SamplerState;
    TComPtr<ID3D11BlendState>   BlendState;
    TComPtr<ID3D11RasterizerState> RasterizerState;
    TComPtr<ID3D11DepthStencilState> DepthStencilState;
};
