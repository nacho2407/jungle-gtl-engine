#include "URenderer.h"

#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include <vector>
#include <cmath>

#include "Component/UCameraComponent.h"
#include "Structs.h"
#include "Resource/UStaticMesh.h"

namespace
{
    constexpr float Pi = 3.14159265358979323846f;

    static std::vector<FVertexSimple> CreateCubeVertices()
    {
        const FVector4 Red = FVector4(1, 0, 0, 1);
        const FVector4 Green = FVector4(0, 1, 0, 1);
        const FVector4 Blue = FVector4(0, 0, 1, 1);
        const FVector4 White = FVector4(1, 1, 1, 1);
        const FVector4 Yellow = FVector4(1, 1, 0, 1);
        const FVector4 Cyan = FVector4(0, 1, 1, 1);

        const float s = 0.5f;

        std::vector<FVertexSimple> V =
        {
            // Front
            { FVector(-s, -s, -s), Red },   { FVector(-s, +s, -s), Red },   { FVector(+s, +s, -s), Red },
            { FVector(-s, -s, -s), Red },   { FVector(+s, +s, -s), Red },   { FVector(+s, -s, -s), Red },

            // Back
            { FVector(-s, -s, +s), Green }, { FVector(+s, +s, +s), Green }, { FVector(-s, +s, +s), Green },
            { FVector(-s, -s, +s), Green }, { FVector(+s, -s, +s), Green }, { FVector(+s, +s, +s), Green },

            // Left
            { FVector(-s, -s, +s), Blue },  { FVector(-s, +s, +s), Blue },  { FVector(-s, +s, -s), Blue },
            { FVector(-s, -s, +s), Blue },  { FVector(-s, +s, -s), Blue },  { FVector(-s, -s, -s), Blue },

            // Right
            { FVector(+s, -s, +s), White }, { FVector(+s, +s, -s), White }, { FVector(+s, +s, +s), White },
            { FVector(+s, -s, +s), White }, { FVector(+s, -s, -s), White }, { FVector(+s, +s, -s), White },

            // Top
            { FVector(-s, +s, +s), Yellow }, { FVector(+s, +s, +s), Yellow }, { FVector(+s, +s, -s), Yellow },
            { FVector(-s, +s, +s), Yellow }, { FVector(+s, +s, -s), Yellow }, { FVector(-s, +s, -s), Yellow },

            // Bottom
            { FVector(-s, -s, +s), Cyan },  { FVector(+s, -s, -s), Cyan },  { FVector(+s, -s, +s), Cyan },
            { FVector(-s, -s, +s), Cyan },  { FVector(-s, -s, -s), Cyan },  { FVector(+s, -s, -s), Cyan },
        };

        return V;
    }

    static std::vector<FVertexSimple> CreateSphereVertices(int SliceCount = 16, int StackCount = 16)
    {
        std::vector<FVertexSimple> V;

        for (int stack = 0; stack < StackCount; ++stack)
        {
            const float phi0 = Pi * static_cast<float>(stack) / static_cast<float>(StackCount);
            const float phi1 = Pi * static_cast<float>(stack + 1) / static_cast<float>(StackCount);

            for (int slice = 0; slice < SliceCount; ++slice)
            {
                const float theta0 = 2.0f * Pi * static_cast<float>(slice) / static_cast<float>(SliceCount);
                const float theta1 = 2.0f * Pi * static_cast<float>(slice + 1) / static_cast<float>(SliceCount);

                auto MakePos = [](float Phi, float Theta) -> FVector
                    {
                        const float x = std::sinf(Phi) * std::cosf(Theta);
                        const float y = std::cosf(Phi);
                        const float z = std::sinf(Phi) * std::sinf(Theta);
                        return FVector(x, y, z);
                    };

                const FVector P00 = MakePos(phi0, theta0);
                const FVector P01 = MakePos(phi0, theta1);
                const FVector P10 = MakePos(phi1, theta0);
                const FVector P11 = MakePos(phi1, theta1);

                const FVector4 C0(0.8f, 0.8f, 1.0f, 1.0f);
                const FVector4 C1(0.6f, 0.9f, 1.0f, 1.0f);

                V.push_back({ P00, C0 });
                V.push_back({ P10, C0 });
                V.push_back({ P11, C0 });

                V.push_back({ P00, C1 });
                V.push_back({ P11, C1 });
                V.push_back({ P01, C1 });
            }
        }

        return V;
    }

    static ID3DBlob* CompileShaderOrNull(const wchar_t* ShaderPath, const char* EntryPoint, const char* Target)
    {
        ID3DBlob* ShaderBlob = nullptr;
        ID3DBlob* ErrorBlob = nullptr;

        const HRESULT Hr = D3DCompileFromFile(
            ShaderPath,
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            EntryPoint,
            Target,
            0,
            0,
            &ShaderBlob,
            &ErrorBlob);

        if (FAILED(Hr))
        {
            if (ErrorBlob)
            {
                ErrorBlob->Release();
            }
            return nullptr;
        }

        if (ErrorBlob)
        {
            ErrorBlob->Release();
        }

        return ShaderBlob;
    }
}

URenderer::URenderer()
{
}

URenderer::~URenderer()
{
    Release();
}

bool URenderer::Create(HWND hWindow)
{
    if (!CreateDeviceAndSwapChain(hWindow)) { return false; }
    if (!CreateFrameBuffer()) { return false; }

    DXGI_SWAP_CHAIN_DESC Desc = {};
    SwapChain->GetDesc(&Desc);

    if (!CreateDepthStencilBuffer(Desc.BufferDesc.Width, Desc.BufferDesc.Height)) return false;
    if (!CreateRasterizerState()) return false;
    if (!CreateShader(L"Render\\Public\\Shaders\\Default.hlsl")) return false;
    if (!CreateConstantBuffer()) return false;

    return true;
}

void URenderer::Release()
{
    if (DeviceContext)
    {
        DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    }

    ReleaseMeshResources();
    ReleaseConstantBuffer();
    ReleaseShader();
    ReleaseRasterizerState();
    ReleaseDepthStencilBuffer();
    ReleaseFrameBuffer();
    ReleaseDeviceAndSwapChain();
}

bool URenderer::CreateDeviceAndSwapChain(HWND hWindow)
{
    D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_0 };

    DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
    SwapChainDesc.BufferDesc.Width = 0;
    SwapChainDesc.BufferDesc.Height = 0;
    SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    SwapChainDesc.SampleDesc.Count = 1;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.BufferCount = 2;
    SwapChainDesc.OutputWindow = hWindow;
    SwapChainDesc.Windowed = TRUE;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    const HRESULT Hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
#if defined(_DEBUG)
        D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
#else
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
#endif
        FeatureLevels,
        ARRAYSIZE(FeatureLevels),
        D3D11_SDK_VERSION,
        &SwapChainDesc,
        &SwapChain,
        &Device,
        nullptr,
        &DeviceContext);

    if (FAILED(Hr))
    {
        return false;
    }

    SwapChain->GetDesc(&SwapChainDesc);
    ViewportInfo = { 0.0f, 0.0f, static_cast<float>(SwapChainDesc.BufferDesc.Width), static_cast<float>(SwapChainDesc.BufferDesc.Height), 0.0f, 1.0f };

    return true;
}

void URenderer::ReleaseDeviceAndSwapChain()
{
    if (DeviceContext)
    {
        DeviceContext->Flush();
    }

    if (SwapChain)
    {
        SwapChain->Release();
        SwapChain = nullptr;
    }

    if (DeviceContext)
    {
        DeviceContext->Release();
        DeviceContext = nullptr;
    }

    if (Device)
    {
        Device->Release();
        Device = nullptr;
    }
}

bool URenderer::CreateFrameBuffer()
{
    if (!SwapChain || !Device)
    {
        return false;
    }

    HRESULT Hr = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&FrameBuffer));
    if (FAILED(Hr))
    {
        return false;
    }

    D3D11_RENDER_TARGET_VIEW_DESC RTVDesc = {};
    RTVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    Hr = Device->CreateRenderTargetView(FrameBuffer, &RTVDesc, &FrameBufferRTV);
    return SUCCEEDED(Hr);
}

void URenderer::ReleaseFrameBuffer()
{
    if (FrameBufferRTV)
    {
        FrameBufferRTV->Release();
        FrameBufferRTV = nullptr;
    }

    if (FrameBuffer)
    {
        FrameBuffer->Release();
        FrameBuffer = nullptr;
    }
}

bool URenderer::CreateDepthStencilBuffer(UINT Width, UINT Height)
{
    if (!Device)
    {
        return false;
    }

    D3D11_TEXTURE2D_DESC Desc = {};
    Desc.Width = Width;
    Desc.Height = Height;
    Desc.MipLevels = 1;
    Desc.ArraySize = 1;
    Desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    Desc.SampleDesc.Count = 1;
    Desc.Usage = D3D11_USAGE_DEFAULT;
    Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    HRESULT Hr = Device->CreateTexture2D(&Desc, nullptr, &DepthStencilBuffer);
    if (FAILED(Hr))
    {
        return false;
    }

    Hr = Device->CreateDepthStencilView(DepthStencilBuffer, nullptr, &DepthStencilView);
    return SUCCEEDED(Hr);
}

void URenderer::ReleaseDepthStencilBuffer()
{
    if (DepthStencilView)
    {
        DepthStencilView->Release();
        DepthStencilView = nullptr;
    }

    if (DepthStencilBuffer)
    {
        DepthStencilBuffer->Release();
        DepthStencilBuffer = nullptr;
    }
}

bool URenderer::CreateRasterizerState()
{
    if (!Device)
    {
        return false;
    }

    D3D11_RASTERIZER_DESC Desc = {};
    Desc.FillMode = D3D11_FILL_SOLID;
    Desc.CullMode = D3D11_CULL_BACK;
    Desc.DepthClipEnable = TRUE;

    return SUCCEEDED(Device->CreateRasterizerState(&Desc, &RasterizerState));
}

void URenderer::ReleaseRasterizerState()
{
    if (RasterizerState)
    {
        RasterizerState->Release();
        RasterizerState = nullptr;
    }
}

bool URenderer::CreateShader(const wchar_t* ShaderPath)
{
    if (!Device)
    {
        return false;
    }

    ID3DBlob* VSBlob = CompileShaderOrNull(ShaderPath, "mainVS", "vs_5_0");
    if (!VSBlob)
    {
        return false;
    }

    ID3DBlob* PSBlob = CompileShaderOrNull(ShaderPath, "mainPS", "ps_5_0");
    if (!PSBlob)
    {
        VSBlob->Release();
        return false;
    }

    HRESULT Hr = Device->CreateVertexShader(VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), nullptr, &SimpleVertexShader);
    if (FAILED(Hr))
    {
        VSBlob->Release();
        PSBlob->Release();
        return false;
    }

    Hr = Device->CreatePixelShader(PSBlob->GetBufferPointer(), PSBlob->GetBufferSize(), nullptr, &SimplePixelShader);
    if (FAILED(Hr))
    {
        VSBlob->Release();
        PSBlob->Release();
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC Layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    Hr = Device->CreateInputLayout(Layout, ARRAYSIZE(Layout), VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), &SimpleInputLayout);

    VSBlob->Release();
    PSBlob->Release();

    return SUCCEEDED(Hr);
}

void URenderer::ReleaseShader()
{
    if (SimpleInputLayout)
    {
        SimpleInputLayout->Release();
        SimpleInputLayout = nullptr;
    }

    if (SimplePixelShader)
    {
        SimplePixelShader->Release();
        SimplePixelShader = nullptr;
    }

    if (SimpleVertexShader)
    {
        SimpleVertexShader->Release();
        SimpleVertexShader = nullptr;
    }
}

bool URenderer::CreateConstantBuffer()
{
    if (!Device)
    {
        return false;
    }

    D3D11_BUFFER_DESC Desc = {};
    Desc.ByteWidth = (sizeof(FVSConstants) + 0xF) & 0xFFFFFFF0;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    return SUCCEEDED(Device->CreateBuffer(&Desc, nullptr, &ConstantBuffer));
}

void URenderer::ReleaseMeshResources()
{
    for (auto& Pair : MeshResourceMap)
    {
        if (Pair.second.VertexBuffer)
        {
            Pair.second.VertexBuffer->Release();
            Pair.second.VertexBuffer = nullptr;
        }

        if (Pair.second.IndexBuffer)
        {
            Pair.second.IndexBuffer->Release();
            Pair.second.IndexBuffer = nullptr;
        }
    }

    MeshResourceMap.clear();
}

void URenderer::ReleaseConstantBuffer()
{
    if (ConstantBuffer)
    {
        ConstantBuffer->Release();
        ConstantBuffer = nullptr;
    }
}

void URenderer::PreparePipeline()
{
    DeviceContext->ClearRenderTargetView(FrameBufferRTV, ClearColor);
    DeviceContext->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    DeviceContext->IASetInputLayout(SimpleInputLayout);

    DeviceContext->VSSetShader(SimpleVertexShader, nullptr, 0);
    DeviceContext->PSSetShader(SimplePixelShader, nullptr, 0);

    DeviceContext->RSSetViewports(1, &ViewportInfo);
    DeviceContext->RSSetState(RasterizerState);

    DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, DepthStencilView);

    if (ConstantBuffer)
    {
        DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
    }
}

void URenderer::UpdateVSConstants(const FMatrix& World, const FMatrix& View, const FMatrix& Projection, const FVector4& Color)
{
    if (!ConstantBuffer)
    {
        return;
    }

    D3D11_MAPPED_SUBRESOURCE Mapped = {};
    if (FAILED(DeviceContext->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
    {
        return;
    }

    FVSConstants* Constants = reinterpret_cast<FVSConstants*>(Mapped.pData);
    Constants->World = World;
    Constants->View = View;
    Constants->Projection = Projection;
    Constants->Color = Color;

    DeviceContext->Unmap(ConstantBuffer, 0);
}

void URenderer::BeginFrame()
{
    PreparePipeline();
}

void URenderer::Render(const FScene& Scene, const UCameraComponent* Camera)
{
    if (!Device || !DeviceContext || !FrameBufferRTV)
    {
        return;
    }

    FMatrix View = FMatrix::Identity;
    FMatrix Projection = FMatrix::Identity;

    if (Camera)
    {
        View = FMatrix::MakeViewMatrix(
            Camera->GetRelativeLocation(),
            Camera->GetRelativeRotation());

        Projection = FMatrix::MakePerspectiveMatrix(
            Camera->GetFieldOfView(),
            Camera->GetAspectRatio(),
            Camera->GetNearClip(),
            Camera->GetFarClip());
    }

    for (const FRenderItem& Item : Scene.RenderItems)
    {
        DrawMeshItem(Item, View, Projection);
    }
}

void URenderer::EndFrame()
{
    if (SwapChain)
    {
        SwapChain->Present(1, 0);
    }
}

void URenderer::Resize(UINT Width, UINT Height)
{
    if (!SwapChain || !DeviceContext)
    {
        return;
    }

    DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

    ReleaseDepthStencilBuffer();
    ReleaseFrameBuffer();

    SwapChain->ResizeBuffers(0, Width, Height, DXGI_FORMAT_UNKNOWN, 0);

    CreateFrameBuffer();
    CreateDepthStencilBuffer(Width, Height);

    ViewportInfo.TopLeftX = 0.0f;
    ViewportInfo.TopLeftY = 0.0f;
    ViewportInfo.Width = static_cast<float>(Width);
    ViewportInfo.Height = static_cast<float>(Height);
    ViewportInfo.MinDepth = 0.0f;
    ViewportInfo.MaxDepth = 1.0f;
}

void URenderer::DrawMeshItem(const FRenderItem& Item, const FMatrix& View, const FMatrix& Projection)
{
    if (!Item.Mesh)
    {
        return;
    }

    FMeshGPUResource Resource;
    if (!GetOrCreateMeshResource(Item.Mesh, Resource))
    {
        return;
    }

    UpdateVSConstants(Item.WorldMatrix, View, Projection, FVector4(1, 1, 1, 1));

    UINT Offset = 0;
    DeviceContext->IASetVertexBuffers(0, 1, &Resource.VertexBuffer, &Stride, &Offset);
    DeviceContext->IASetPrimitiveTopology(Resource.Topology);

    if (Resource.IndexBuffer && Resource.IndexCount > 0)
    {
        DeviceContext->IASetIndexBuffer(Resource.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
        DeviceContext->DrawIndexed(Resource.IndexCount, 0, 0);
    }
    else
    {
        DeviceContext->Draw(Resource.VertexCount, 0);
    }
}

bool URenderer::GetOrCreateMeshResource(UStaticMesh* Mesh, FMeshGPUResource& OutResource)
{
    if (!Mesh)
    {
        return false;
    }

    auto Found = MeshResourceMap.find(Mesh);
    if (Found != MeshResourceMap.end())
    {
        OutResource = Found->second;
        return true;
    }

    const std::vector<FVertexSimple>& Vertices = Mesh->GetVertices();
    if (Vertices.empty())
    {
        return false;
    }

    FMeshGPUResource Resource;
    Resource.VertexCount = static_cast<UINT>(Vertices.size());
    Resource.IndexCount = Mesh->GetIndexCount();
    Resource.Topology = ConvertTopology(Mesh->GetTopology());

    D3D11_BUFFER_DESC VBDesc = {};
    VBDesc.ByteWidth = static_cast<UINT>(Vertices.size() * sizeof(FVertexSimple));
    VBDesc.Usage = D3D11_USAGE_IMMUTABLE;
    VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA VBData = {};
    VBData.pSysMem = Vertices.data();

    if (FAILED(Device->CreateBuffer(&VBDesc, &VBData, &Resource.VertexBuffer)))
    {
        return false;
    }

    if (Mesh->HasIndices())
    {
        const std::vector<uint32_t>& Indices = Mesh->GetIndices();

        D3D11_BUFFER_DESC IBDesc = {};
        IBDesc.ByteWidth = static_cast<UINT>(Indices.size() * sizeof(uint32_t));
        IBDesc.Usage = D3D11_USAGE_IMMUTABLE;
        IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA IBData = {};
        IBData.pSysMem = Indices.data();

        if (FAILED(Device->CreateBuffer(&IBDesc, &IBData, &Resource.IndexBuffer)))
        {
            Resource.VertexBuffer->Release();
            Resource.VertexBuffer = nullptr;
            return false;
        }
    }

    MeshResourceMap[Mesh] = Resource;
    OutResource = Resource;
    return true;
}

D3D11_PRIMITIVE_TOPOLOGY URenderer::ConvertTopology(EMeshTopology Topology) const
{
    switch (Topology)
    {
    case EMeshTopology::LineList:
        return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;

    case EMeshTopology::TriangleList:
    default:
        return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}