#include "D3D11WidgetRenderer.h"
#include "Renderer/D3D11/D3D11RHI.h"

bool FD3D11WidgetRenderer::Initialize(FD3D11RHI* InRHI) 
{ 
    if (!InRHI)
    {
        return false;
    }

	RHI = InRHI;
    const wchar_t* ShaderPath = L"Content\\Shader\\ShaderMesh.hlsl";	// Add a new shader if necessary

	// Shader
	D3D11_INPUT_ELEMENT_DESC LayoutDesc[] = { 
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
		 D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	RHI->CreateVertexShaderAndInputLayout(ShaderPath, "VSMain", LayoutDesc, 1,
                                              VertexShader.GetAddressOf(),
                                              InputLayout.GetAddressOf());
	RHI->CreatePixelShader(ShaderPath, "PSMain", PixelShader.GetAddressOf());

	FVector QuadVert[4] = {
		{0.f, 0.f, 0.f},
		{1.f, 0.f, 0.f},
		{1.f, 1.f, 0.f}, 
		{0.f, 1.f, 0.f},
	};
    uint16 Indices[6] = {0, 1, 3, 1, 2, 3};
	
	RHI->CreateVertexBuffer(QuadVert, sizeof(QuadVert) * 4, sizeof(QuadVert), false, VertexBuffer.GetAddressOf());
    RHI->CreateIndexBuffer(Indices, sizeof(Indices), false, IndexBuffer.GetAddressOf());
    RHI->CreateConstantBuffer(sizeof(FMeshLitConstants), ConstantBuffer.GetAddressOf());

	// Render states
    D3D11_DEPTH_STENCIL_DESC DS_DESC = {};
    DS_DESC.DepthEnable    = FALSE;
    DS_DESC.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    DS_DESC.DepthFunc      = D3D11_COMPARISON_ALWAYS;
    RHI->CreateDepthStencilState(DS_DESC, DepthStencilState.GetAddressOf());

	// Alpha blend
    D3D11_BLEND_DESC BlendDesc = {};
    BlendDesc.RenderTarget[0].BlendEnable           = TRUE;
    BlendDesc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
    BlendDesc.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    BlendDesc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    BlendDesc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
    BlendDesc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    RHI->CreateBlendState(BlendDesc, BlendState.GetAddressOf());

	// Rasterizer state
    D3D11_RASTERIZER_DESC RasterizerDesc = {};
    RasterizerDesc.CullMode = D3D11_CULL_BACK;
    RasterizerDesc.FillMode = D3D11_FILL_SOLID;
    RasterizerDesc.DepthClipEnable = TRUE;
    RHI->CreateRasterizerState(RasterizerDesc, RasterizerState.GetAddressOf());

    return true;
}

void FD3D11WidgetRenderer::BeginFrame(uint32 ScreenWidth, uint32 ScreenHeight)
{
    const float W = static_cast<float>(ScreenWidth);
    const float H = static_cast<float>(ScreenHeight);

    // Reset the D3D11 viewport to the full window — panel rendering leaves it
    // set to the last panel's sub-region, which would misplace widget draws.
    RHI->SetViewport(static_cast<int32>(ScreenWidth), static_cast<int32>(ScreenHeight));

    OrthographicMatrix = FMatrix(
        2.f/W, 0.f,   0.f,  0.f,
        0.f,  -2.f/H, 0.f,  0.f,
        0.f,   0.f,   1.f,  0.f,
        -1.f,  1.f,   0.f,  1.f);
}

void FD3D11WidgetRenderer::DrawWidget(ID3D11DeviceContext* Context, float X, float Y, float W,
                                      float H, FColor Color)
{
    FMatrix Model(
        W, 0.f, 0.f, 0.f,
        0.f, H, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        X,   Y, 0.f, 1.f);

    FMeshLitConstants CB = {}; // FMeshLitConstants로 교체
    CB.MVP       = Model * OrthographicMatrix;
    CB.World     = FMatrix::Identity;
    CB.bEnableLighting = 0; // 위젯은 라이팅 비활성화
    CB.BaseColor = Color;
    RHI->UpdateConstantBuffer(ConstantBuffer.Get(), &CB, sizeof(CB));

    Context->VSSetShader(VertexShader.Get(), nullptr, 0);
    Context->PSSetShader(PixelShader.Get(), nullptr, 0);
    Context->IASetInputLayout(InputLayout.Get());
    Context->VSSetConstantBuffers(0, 1, ConstantBuffer.GetAddressOf());
    Context->PSSetConstantBuffers(0, 1, ConstantBuffer.GetAddressOf());
    Context->OMSetDepthStencilState(DepthStencilState.Get(), 0);
    Context->OMSetBlendState(BlendState.Get(), nullptr, 0xffffffff);
    Context->RSSetState(RasterizerState.Get());

    constexpr UINT Stride = sizeof(FVector);
    constexpr UINT Offset = 0;
    Context->IASetVertexBuffers(0, 1, VertexBuffer.GetAddressOf(), &Stride, &Offset);
    Context->IASetIndexBuffer(IndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context->DrawIndexed(6, 0, 0);
}

void FD3D11WidgetRenderer::Shutdown()
{
    RasterizerState.Reset();
    BlendState.Reset();
    DepthStencilState.Reset();
    IndexBuffer.Reset();
    VertexBuffer.Reset();
    ConstantBuffer.Reset();
    InputLayout.Reset();
    PixelShader.Reset();
    VertexShader.Reset();
    RHI = nullptr;
}