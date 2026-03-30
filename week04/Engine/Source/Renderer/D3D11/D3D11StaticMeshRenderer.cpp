#include "Renderer/D3D11/D3D11StaticMeshRenderer.h"
#include "Renderer/D3D11/D3D11RHI.h"
#include "Renderer/SceneView.h"
#include "Renderer/Types/ShaderConstants.h"
#include "Renderer/Types/VertexTypes.h" // FMeshVertexPNCT 등
#include "Asset/MaterialInterface.h"
#include "Renderer/RenderAsset/MaterialResource.h"

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
    if (!CreateDefaultResources())
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

    LinearSamplerState.Reset();
    DefaultWhiteSRV.Reset();
    DefaultWhiteTexture.Reset();

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
        //FMeshUnlitConstants Constants = {};
        //Constants.MVP = DrawItem.World * CurrentPassParams.SceneView->GetViewProjectionMatrix();


        //if (!RHI->UpdateConstantBuffer(ConstantBuffer.Get(), &Constants, sizeof(Constants)))
        //{
        //    continue;
        //}

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

            const FMaterialData* MaterialData = nullptr;

            if (i < DrawItem.MaterialBindings.size())
            {
                const FStaticMeshMaterialBinding& Binding = DrawItem.MaterialBindings[i];
                if (Binding.Material != nullptr)
                {
                    MaterialData = Binding.Material->GetMaterialData(Binding.SubMaterialName);
                }
            }

            FMeshUnlitConstants Constants = {};
            Constants.MVP = DrawItem.World * CurrentPassParams.SceneView->GetViewProjectionMatrix();

            if (MaterialData)
            {
                Constants.BaseColor =
                    FColor(MaterialData->DiffuseColor.X, MaterialData->DiffuseColor.Y,
                           MaterialData->DiffuseColor.Z, MaterialData->Opacity);
            }
            else
            {
                Constants.BaseColor = FColor::White();
            }

            if (!RHI->UpdateConstantBuffer(ConstantBuffer.Get(), &Constants, sizeof(Constants)))
            {
                continue;
            }
            /*if (CurrentPassParams.ViewMode == EViewModeIndex::VMI_Wireframe)
            {
                BindDefaultMaterial();
            }
            else
            {
                BindMaterial(MaterialData);
            }*/
            BindMaterial(MaterialData);
            RHI->DrawIndexed(Sub.IndexCount, Sub.StartIndexLocation, 0);
            BindDefaultMaterial();
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

void FD3D11StaticMeshRenderer::BindMaterial(const FMaterialData* InMaterialData)
{
    if (RHI == nullptr)
    {
        return;
    }

    ID3D11ShaderResourceView* DiffuseSRV = DefaultWhiteSRV.Get();

    if (InMaterialData != nullptr && InMaterialData->DiffuseTexture != nullptr)
    {
        DiffuseSRV = InMaterialData->DiffuseTexture->GetSRV();
        if (DiffuseSRV == nullptr)
        {
            DiffuseSRV = DefaultWhiteSRV.Get();
        }
    }

    RHI->SetPSShaderResource(0, DiffuseSRV);

    ID3D11SamplerState* Sampler = LinearSamplerState.Get();
    RHI->GetDeviceContext()->PSSetSamplers(0, 1, &Sampler);
}

void FD3D11StaticMeshRenderer::BindDefaultMaterial()
{
    if (RHI == nullptr)
    {
        return;
    }

    ID3D11ShaderResourceView* DiffuseSRV = DefaultWhiteSRV.Get();
    RHI->SetPSShaderResource(0, DiffuseSRV);

    ID3D11SamplerState* Sampler = LinearSamplerState.Get();
    RHI->GetDeviceContext()->PSSetSamplers(0, 1, &Sampler);
}

bool FD3D11StaticMeshRenderer::CreateDefaultResources()
{
    if (RHI == nullptr || RHI->GetDevice() == nullptr)
    {
        return false;
    }

    ID3D11Device* Device = RHI->GetDevice();

    // 1x1 RGBA white texture
    const uint32 WhitePixel = 0xffffffff;

    D3D11_TEXTURE2D_DESC TexDesc = {};
    TexDesc.Width = 1;
    TexDesc.Height = 1;
    TexDesc.MipLevels = 1;
    TexDesc.ArraySize = 1;
    TexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    TexDesc.SampleDesc.Count = 1;
    TexDesc.Usage = D3D11_USAGE_IMMUTABLE;
    TexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA InitData = {};
    InitData.pSysMem = &WhitePixel;
    InitData.SysMemPitch = sizeof(uint32);

    if (FAILED(Device->CreateTexture2D(&TexDesc, &InitData, DefaultWhiteTexture.GetAddressOf())))
    {
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = TexDesc.Format;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels = 1;

    if (FAILED(Device->CreateShaderResourceView(DefaultWhiteTexture.Get(), &SRVDesc,
                                                DefaultWhiteSRV.GetAddressOf())))
    {
        return false;
    }

    D3D11_SAMPLER_DESC SamplerDesc = {};
    SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    SamplerDesc.MinLOD = 0.0f;
    SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SamplerDesc.MaxAnisotropy = 1;

    if (FAILED(Device->CreateSamplerState(&SamplerDesc, LinearSamplerState.GetAddressOf())))
    {
        return false;
    }

    return true;
}