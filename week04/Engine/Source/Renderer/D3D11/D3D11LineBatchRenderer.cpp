#include "Renderer/D3D11/D3D11LineBatchRenderer.h"

#include "Renderer/D3D11/D3D11RHI.h"
#include "Renderer/SceneView.h"
#include "Renderer/Types/ShaderConstants.h"

bool FD3D11LineBatchRenderer::Initialize(FD3D11RHI* InRHI)
{
    if (InRHI == nullptr)
    {
        return false;
    }

    RHI = InRHI;
    CurrentSceneView = nullptr;

    Vertices.reserve(MaxVertexCount);

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

    if (!CreateDynamicVertexBuffer(MaxVertexCount))
    {
        Shutdown();
        return false;
    }

    return true;
}

void FD3D11LineBatchRenderer::Shutdown()
{
    BlendState.Reset();

    RasterizerState.Reset();
    DepthStencilState.Reset();

    DynamicVertexBuffer.Reset();
    ConstantBuffer.Reset();
    InputLayout.Reset();
    PixelShader.Reset();
    VertexShader.Reset();

    Vertices.clear();
    CurrentSceneView = nullptr;
    RHI = nullptr;
}

void FD3D11LineBatchRenderer::BeginFrame(const FSceneView* InSceneView)
{
    CurrentSceneView = InSceneView;
    Vertices.clear();
}

void FD3D11LineBatchRenderer::AddLine(const FVector& InStart, const FVector& InEnd,
                                      const FColor& InColor)
{
    if (RHI == nullptr || CurrentSceneView == nullptr)
    {
        return;
    }

    if (Vertices.size() + 2 > MaxVertexCount)
    {
        Flush();
    }

    if (Vertices.size() + 2 > MaxVertexCount)
    {
        return;
    }

    FLineVertex V0 = {};
    V0.Position = InStart;
    V0.Color = InColor;

    FLineVertex V1 = {};
    V1.Position = InEnd;
    V1.Color = InColor;

    Vertices.push_back(V0);
    Vertices.push_back(V1);
}

void FD3D11LineBatchRenderer::EndFrame()
{
    if (RHI == nullptr)
    {
        return;
    }

    Flush();
    CurrentSceneView = nullptr;
}

bool FD3D11LineBatchRenderer::CreateShaders()
{
    if (RHI == nullptr)
    {
        return false;
    }

    static const D3D11_INPUT_ELEMENT_DESC InputElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         static_cast<UINT>(offsetof(FLineVertex, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         static_cast<UINT>(offsetof(FLineVertex, Color)), D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    if (!RHI->CreateVertexShaderAndInputLayout(DefaultShaderPath, "VSMain", InputElements,
                                               static_cast<uint32>(ARRAYSIZE((InputElements))),
                                               VertexShader.GetAddressOf(),
                                               InputLayout.GetAddressOf()))
    {
        return false;
    }

    if (!RHI->CreatePixelShader(DefaultShaderPath, "PSMain", PixelShader.GetAddressOf()))
    {
        VertexShader.Reset();
        InputLayout.Reset();
        return false;
    }

    return true;
}

bool FD3D11LineBatchRenderer::CreateConstantBuffer()
{
    if (RHI == nullptr)
    {
        return false;
    }

    return RHI->CreateConstantBuffer(sizeof(FLineConstants), ConstantBuffer.GetAddressOf());
}

bool FD3D11LineBatchRenderer::CreateDynamicVertexBuffer(uint32 InMaxVertexCount)
{
    if (RHI == nullptr || RHI->GetDevice() == nullptr)
    {
        return false;
    }

    D3D11_BUFFER_DESC Desc = {};
    Desc.ByteWidth = sizeof(FLineVertex) * InMaxVertexCount;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;
    Desc.StructureByteStride = 0;

    return SUCCEEDED(
        RHI->GetDevice()->CreateBuffer(&Desc, nullptr, DynamicVertexBuffer.GetAddressOf()));
}

void FD3D11LineBatchRenderer::Flush()
{
    if (RHI == nullptr || CurrentSceneView == nullptr)
    {
        return;
    }

    if (Vertices.empty())
    {
        return;
    }

    if (!RHI->UpdateDynamicBuffer(DynamicVertexBuffer.Get(), Vertices.data(),
                                  static_cast<uint32>(sizeof(FLineVertex) * Vertices.size())))
    {
        return;
    }

    FLineConstants Constants = {};
    Constants.VP = CurrentSceneView->GetViewProjectionMatrix();
    FVector CameraPos = CurrentSceneView->GetViewLocation();
    Constants.CameraPos[0] = CameraPos.X;
    Constants.CameraPos[1] = CameraPos.Y;
    Constants.CameraPos[2] = CameraPos.Z;
    Constants.MaxDistance = 1000.0f;

    if (!RHI->UpdateConstantBuffer(ConstantBuffer.Get(), &Constants, sizeof(Constants)))
    {
        return;
    }

    const UINT    Stride = sizeof(FLineVertex);
    const UINT    Offset = 0;
    ID3D11Buffer* VertexBuffer = DynamicVertexBuffer.Get();

    RHI->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    RHI->SetInputLayout(InputLayout.Get());
    RHI->SetVertexBuffer(0, VertexBuffer, Stride, Offset);

    RHI->SetVertexShader(VertexShader.Get());
    RHI->SetVSConstantBuffer(0, ConstantBuffer.Get());

    RHI->SetPixelShader(PixelShader.Get());

    RHI->SetRasterizerState(RasterizerState.Get());
    RHI->SetDepthStencilState(DepthStencilState.Get(), 0);

    float BlendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    UINT  SampleMask = 0xffffffff;
    RHI->SetBlendState(BlendState.Get(), BlendFactor, 0xffffffff);

    RHI->Draw(static_cast<uint32>(Vertices.size()), 0);

    RHI->GetDeviceContext()->OMSetBlendState(nullptr, BlendFactor, SampleMask);
    Vertices.clear();
}

bool FD3D11LineBatchRenderer::CreateStates()
{
    if (RHI == nullptr)
    {
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC DepthDesc = {};
    DepthDesc.DepthEnable = TRUE;
    DepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    DepthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    DepthDesc.StencilEnable = FALSE;

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

    if (!RHI->CreateRasterizerState(RasterizerDesc, RasterizerState.GetAddressOf()))
    {
        return false;
    }

    D3D11_BLEND_DESC BlendDesc = {};
    BlendDesc.AlphaToCoverageEnable = FALSE;
    BlendDesc.IndependentBlendEnable = FALSE;
    BlendDesc.RenderTarget[0].BlendEnable = TRUE;
    BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    return RHI->CreateBlendState(BlendDesc, BlendState.GetAddressOf());
}
