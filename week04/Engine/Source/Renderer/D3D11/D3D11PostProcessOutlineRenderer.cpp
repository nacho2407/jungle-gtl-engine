#include "Renderer/D3D11/D3D11PostProcessOutlineRenderer.h"

#include "Renderer/D3D11/D3D11RHI.h"
#include "Renderer/MemoryTracker.h"
#include "Renderer/SceneView.h"

namespace
{
    struct alignas(16) FOutlinePostProcessConstants
    {
        float MaskTexelSizeX = 0.0f;
        float MaskTexelSizeY = 0.0f;
        float Thickness = 1.0f;
        float Padding0 = 0.0f;
        float ViewMinU = 0.0f;
        float ViewMinV = 0.0f;
        float ViewSizeU = 1.0f;
        float ViewSizeV = 1.0f;
        FColor OutlineColor = FColor(1.0f, 0.72f, 0.20f, 1.0f);
    };
}

bool FD3D11PostProcessOutlineRenderer::Initialize(FD3D11RHI* InRHI)
{
    if (InRHI == nullptr)
    {
        return false;
    }

    RHI = InRHI;
    CurrentSceneView = nullptr;
    CurrentMaskSRV = nullptr;

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

    return true;
}

void FD3D11PostProcessOutlineRenderer::Shutdown()
{
    DepthStencilState.Reset();
    RasterizerState.Reset();
    BlendState.Reset();
    SamplerState.Reset();
    ConstantBuffer.Reset();
    PixelShader.Reset();
    VertexShader.Reset();

    CurrentSceneView = nullptr;
    CurrentMaskSRV = nullptr;
    RHI = nullptr;
}

void FD3D11PostProcessOutlineRenderer::BeginFrame(const FSceneView* InSceneView,
                                                  ID3D11ShaderResourceView* InMaskSRV)
{
    CurrentSceneView = InSceneView;
    CurrentMaskSRV = InMaskSRV;
}

void FD3D11PostProcessOutlineRenderer::EndFrame()
{
    if (RHI == nullptr || CurrentSceneView == nullptr || CurrentMaskSRV == nullptr)
    {
        CurrentSceneView = nullptr;
        CurrentMaskSRV = nullptr;
        return;
    }

    const FViewportRect& ViewRect = CurrentSceneView->GetViewRect();
    const float RenderTargetWidth = static_cast<float>(RHI->GetViewportWidth());
    const float RenderTargetHeight = static_cast<float>(RHI->GetViewportHeight());

    RHI->SetDefaultRenderTargets();

    D3D11_VIEWPORT Viewport = {};
    Viewport.TopLeftX = static_cast<float>(ViewRect.X);
    Viewport.TopLeftY = static_cast<float>(ViewRect.Y);
    Viewport.Width = static_cast<float>(ViewRect.Width);
    Viewport.Height = static_cast<float>(ViewRect.Height);
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;
    RHI->SetViewport(Viewport);

    FOutlinePostProcessConstants Constants = {};
    Constants.MaskTexelSizeX =
        (RenderTargetWidth > 0.0f) ? (1.0f / RenderTargetWidth) : 0.0f;
    Constants.MaskTexelSizeY =
        (RenderTargetHeight > 0.0f) ? (1.0f / RenderTargetHeight) : 0.0f;
    Constants.Thickness = 2.5f;
    Constants.ViewMinU =
        (RenderTargetWidth > 0.0f) ? (static_cast<float>(ViewRect.X) / RenderTargetWidth) : 0.0f;
    Constants.ViewMinV =
        (RenderTargetHeight > 0.0f) ? (static_cast<float>(ViewRect.Y) / RenderTargetHeight) : 0.0f;
    Constants.ViewSizeU =
        (RenderTargetWidth > 0.0f) ? (static_cast<float>(ViewRect.Width) / RenderTargetWidth) : 0.0f;
    Constants.ViewSizeV = (RenderTargetHeight > 0.0f)
                              ? (static_cast<float>(ViewRect.Height) / RenderTargetHeight)
                              : 0.0f;

    if (!RHI->UpdateConstantBuffer(ConstantBuffer.Get(), &Constants, sizeof(Constants)))
    {
        CurrentSceneView = nullptr;
        CurrentMaskSRV = nullptr;
        return;
    }

    const float BlendFactor[4] = {0.f, 0.f, 0.f, 0.f};

    RHI->SetInputLayout(nullptr);
    RHI->SetVertexShader(VertexShader.Get());
    RHI->SetPixelShader(PixelShader.Get());
    RHI->SetVSConstantBuffer(0, ConstantBuffer.Get());
    RHI->SetPSConstantBuffer(0, ConstantBuffer.Get());
    RHI->SetRasterizerState(RasterizerState.Get());
    RHI->SetDepthStencilState(DepthStencilState.Get(), 0);
    RHI->SetBlendState(BlendState.Get(), BlendFactor);
    RHI->SetPSSampler(0, SamplerState.Get());
    RHI->SetPSShaderResource(0, CurrentMaskSRV);
    RHI->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    RHI->Draw(3, 0);

    RHI->ClearPSShaderResource(0);
    RHI->ClearBlendState();

    CurrentSceneView = nullptr;
    CurrentMaskSRV = nullptr;
}

bool FD3D11PostProcessOutlineRenderer::CreateShaders()
{
    if (RHI == nullptr)
    {
        return false;
    }

    TComPtr<ID3DBlob> VertexShaderBlob;
    if (!RHI->CompileShaderFromFile(ShaderPath, "VSMain", "vs_5_0",
                                    VertexShaderBlob.GetAddressOf()))
    {
        return false;
    }

    if (FAILED(RHI->GetDevice()->CreateVertexShader(VertexShaderBlob->GetBufferPointer(),
                                                    VertexShaderBlob->GetBufferSize(), nullptr,
                                                    VertexShader.GetAddressOf())))
    {
        return false;
    }

    GMemoryTracker.AddVertexShaderBlobBytes(static_cast<uint64>(VertexShaderBlob->GetBufferSize()));

    if (!RHI->CreatePixelShader(ShaderPath, "PSMain", PixelShader.GetAddressOf()))
    {
        VertexShader.Reset();
        return false;
    }

    return true;
}

bool FD3D11PostProcessOutlineRenderer::CreateConstantBuffer()
{
    return (RHI != nullptr) &&
           RHI->CreateConstantBuffer(sizeof(FOutlinePostProcessConstants),
                                     ConstantBuffer.GetAddressOf());
}

bool FD3D11PostProcessOutlineRenderer::CreateStates()
{
    if (RHI == nullptr)
    {
        return false;
    }

    D3D11_SAMPLER_DESC SamplerDesc = {};
    SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SamplerDesc.MinLOD = 0;
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

    if (!RHI->CreateBlendState(BlendDesc, BlendState.GetAddressOf()))
    {
        return false;
    }

    D3D11_RASTERIZER_DESC RasterizerDesc = {};
    RasterizerDesc.FillMode = D3D11_FILL_SOLID;
    RasterizerDesc.CullMode = D3D11_CULL_NONE;
    RasterizerDesc.DepthClipEnable = TRUE;

    if (!RHI->CreateRasterizerState(RasterizerDesc, RasterizerState.GetAddressOf()))
    {
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC DepthDesc = {};
    DepthDesc.DepthEnable = FALSE;
    DepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;

    if (!RHI->CreateDepthStencilState(DepthDesc, DepthStencilState.GetAddressOf()))
    {
        return false;
    }

    return true;
}
