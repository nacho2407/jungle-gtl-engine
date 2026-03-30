#include "Renderer/D3D11/D3D11StaticMeshRenderer.h"
#include "Renderer/D3D11/D3D11RHI.h"
#include "Renderer/SceneView.h"
#include "Renderer/Types/ShaderConstants.h"
#include "Renderer/Types/VertexTypes.h" // FMeshVertexPNCT 등
#include "Asset/MaterialInterface.h"

bool FD3D11StaticMeshRenderer::Initialize(FD3D11RHI* InRHI)
{
    if (InRHI == nullptr)
    {
        return false;
    }

    RHI = InRHI;
    CurrentPassParams = {};

    if (!CreateShaders())
        return false;
    if (!CreateConstantBuffers())
        return false;
    if (!CreateStates())
        return false;

    ResetBatches();
    return true;
}

void FD3D11StaticMeshRenderer::Shutdown()
{
    ResetBatches();
    CurrentPassParams = {};

    DepthDisabledState.Reset();
    DepthStencilState.Reset();
    WireframeRasterizerState.Reset();
    SolidRasterizerState.Reset();

    ConstantBuffer.Reset();
    InputLayout.Reset();
    VertexShader.Reset();
    PixelShader.Reset();

    RHI = nullptr;
}

void FD3D11StaticMeshRenderer::BeginFrame(const FMeshPassParams& InPassParams)
{
    CurrentPassParams = InPassParams;
    ResetBatches();
}

void FD3D11StaticMeshRenderer::AddStaticMesh(const FStaticMeshRenderItem& InItem)
{
    if (!InItem.State.IsVisible() || InItem.RenderResource == nullptr)
    {
        return;
    }
    StaticMeshDraws.push_back(InItem);
}

void FD3D11StaticMeshRenderer::AddStaticMeshes(const TArray<FStaticMeshRenderItem>& InItems)
{
    for (const FStaticMeshRenderItem& Item : InItems)
    {
        AddStaticMesh(Item);
    }
}

void FD3D11StaticMeshRenderer::EndFrame()
{
    if (RHI == nullptr)
    {
        return;
    }

    Flush();
    ResetBatches();
    CurrentPassParams = {};
}

void FD3D11StaticMeshRenderer::Flush()
{
    if (RHI == nullptr || CurrentPassParams.SceneView == nullptr || StaticMeshDraws.empty())
    {
        return;
    }

    BindPipeline();

    if (CurrentPassParams.ViewMode == EViewModeIndex::VMI_Wireframe)
    {
        BindWireframeRasterizer();
    }
    else
    {
        BindSolidRasterizer();
    }

    RHI->SetDepthStencilState(
        CurrentPassParams.bDisableDepth ? DepthDisabledState.Get() : DepthStencilState.Get(), 0);
    RHI->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 각 모델별 순회 렌더링
    for (const FStaticMeshRenderItem& DrawItem : StaticMeshDraws)
    {
        const FStaticMeshResource* Resource = DrawItem.RenderResource;
        if (Resource->VertexBuffer == nullptr || Resource->IndexBuffer == nullptr)
        {
            continue;
        }

        // 1. 상수 버퍼 업데이트 (MVP 행렬)
        FMeshUnlitConstants Constants = {};
        Constants.MVP = DrawItem.World * CurrentPassParams.SceneView->GetViewProjectionMatrix();

        // BaseColor는 머티리얼 시스템이 확장되면 머티리얼 상수 버퍼로 분리하는 것이 좋습니다.
        Constants.BaseColor = FColor::White();

        if (!RHI->UpdateConstantBuffer(ConstantBuffer.Get(), &Constants, sizeof(Constants)))
        {
            continue;
        }

        // 2. 해당 모델의 VBO / IBO 바인딩
        const UINT    Stride = Resource->VertexStride;
        const UINT    Offset = 0;
        ID3D11Buffer* VertexBuffer = Resource->VertexBuffer.Get();

        RHI->SetVertexBuffer(0, VertexBuffer, Stride, Offset);
        // 인덱스 타입이 uint32인 경우 DXGI_FORMAT_R32_UINT를 사용 (uint16인 경우 R16_UINT)
        RHI->SetIndexBuffer(Resource->IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

        // 3. 서브 메시 및 머티리얼 단위 렌더링
        const uint32 NumSubMeshes = static_cast<uint32>(Resource->SubMeshes.size());
        for (uint32 i = 0; i < NumSubMeshes; ++i)
        {
            const FSubMesh& Sub = Resource->SubMeshes[i];

            // TODO: 머티리얼 바인딩 로직
            // if (i < DrawItem.Materials.size() && DrawItem.Materials[i])
            // {
            //     DrawItem.Materials[i]->Bind(RHI);
            // }

            RHI->DrawIndexed(Sub.IndexCount, Sub.StartIndexLocation, 0);
        }
    }
}

bool FD3D11StaticMeshRenderer::CreateShaders()
{
    if (RHI == nullptr)
        return false;

    // FMeshVertexPNCT 구조체에 맞춘 입력 레이아웃 (위치, 법선, UV 등)
    // 파싱 구조에 맞게 오프셋과 포맷을 수정해야 합니다.
    static const D3D11_INPUT_ELEMENT_DESC LayoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    if (!RHI->CreateVertexShaderAndInputLayout(
            ShaderPath, "VSMain", LayoutDesc, static_cast<uint32>(ARRAYSIZE(LayoutDesc)),
            VertexShader.GetAddressOf(), InputLayout.GetAddressOf()))
    {
        return false;
    }

    if (!RHI->CreatePixelShader(ShaderPath, "PSMain", PixelShader.GetAddressOf()))
    {
        InputLayout.Reset();
        VertexShader.Reset();
        return false;
    }

    return true;
}

bool FD3D11StaticMeshRenderer::CreateConstantBuffers()
{
    if (RHI == nullptr)
        return false;

    return RHI->CreateConstantBuffer(sizeof(FMeshUnlitConstants), ConstantBuffer.GetAddressOf());
}

bool FD3D11StaticMeshRenderer::CreateStates()
{
    if (RHI == nullptr || RHI->GetDevice() == nullptr)
        return false;

    ID3D11Device* Device = RHI->GetDevice();

    {
        D3D11_RASTERIZER_DESC Desc = {};
        Desc.FillMode = D3D11_FILL_SOLID;
        Desc.CullMode = D3D11_CULL_BACK;
        Desc.FrontCounterClockwise = FALSE;
        Desc.DepthClipEnable = TRUE;

        if (FAILED(Device->CreateRasterizerState(&Desc, SolidRasterizerState.GetAddressOf())))
            return false;

        Desc.FillMode = D3D11_FILL_WIREFRAME;
        if (FAILED(Device->CreateRasterizerState(&Desc, WireframeRasterizerState.GetAddressOf())))
            return false;
    }

    {
        D3D11_DEPTH_STENCIL_DESC Desc = {};
        Desc.DepthEnable = TRUE;
        Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        Desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

        if (FAILED(Device->CreateDepthStencilState(&Desc, DepthStencilState.GetAddressOf())))
            return false;

        D3D11_DEPTH_STENCIL_DESC NoDepthDesc = {};
        NoDepthDesc.DepthEnable = FALSE;
        NoDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        NoDepthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;

        if (FAILED(
                Device->CreateDepthStencilState(&NoDepthDesc, DepthDisabledState.GetAddressOf())))
            return false;
    }

    return true;
}

void FD3D11StaticMeshRenderer::ResetBatches() { StaticMeshDraws.clear(); }

void FD3D11StaticMeshRenderer::BindPipeline()
{
    if (RHI == nullptr)
        return;

    RHI->SetInputLayout(InputLayout.Get());
    RHI->SetVertexShader(VertexShader.Get());
    RHI->SetVSConstantBuffer(0, ConstantBuffer.Get());
    RHI->SetPSConstantBuffer(0, ConstantBuffer.Get());
    RHI->SetPixelShader(PixelShader.Get());
}

void FD3D11StaticMeshRenderer::BindSolidRasterizer()
{
    if (RHI)
        RHI->SetRasterizerState(SolidRasterizerState.Get());
}

void FD3D11StaticMeshRenderer::BindWireframeRasterizer()
{
    if (RHI)
        RHI->SetRasterizerState(WireframeRasterizerState.Get());
}