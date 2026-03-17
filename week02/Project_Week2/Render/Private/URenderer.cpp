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
    if (!CreateDeviceAndSwapChain(hWindow)) { Release(); return false; }
    if (!CreateFrameBuffer()) { Release(); return false; }

    // (X) CreateDeviceAndSwapChain 내부와 중복
    //DXGI_SWAP_CHAIN_DESC Desc = {};
    //SwapChain->GetDesc(&Desc);
    float Width = ViewportInfo.Width;
    float Height = ViewportInfo.Height;

    if (!CreateDepthStencilBuffer(Width, Height)) { Release(); return false; }
    if (!CreateRasterizerStates()) { Release(); return false; }
    if (!CreateDepthStencilStates()) { Release(); return false; }
    if (!CreateShader(L"Render\\Public\\Shaders\\Default.hlsl")) { Release(); return false; }
    if (!CreateConstantBuffer()) { Release(); return false; }

    // for Hit Proxy
    if (!CreateHitProxyBuffer(Width, Height)) { Release(); return false; }
    if (!CreateHitProxyShader(L"Render\\Public\\Shaders\\HitProxy.hlsl")) { Release(); return false; }

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
    ReleaseHitProxyShader(); // for Hit Proxy
    ReleaseShader();
    ReleaseDepthStencilStates();
    ReleaseRasterizerStates();
    ReleaseHitProxyBuffer(); // for Hit Proxy
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
    if (FAILED(Hr))
    {
        FrameBuffer->Release();
        FrameBuffer = nullptr;
        return false;
    }

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
    if (FAILED(Hr))
    {
        DepthStencilBuffer->Release();
        DepthStencilBuffer = nullptr;
        return false;
    }

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

bool URenderer::CreateRasterizerStates()
{
    if (!Device)
    {
        return false;
    }

    D3D11_RASTERIZER_DESC Desc = {};
    Desc.FillMode = D3D11_FILL_SOLID;
    Desc.CullMode = D3D11_CULL_NONE;
    Desc.DepthClipEnable = TRUE;
    Desc.FrontCounterClockwise = TRUE;

    Desc.CullMode = D3D11_CULL_BACK;
    if (FAILED(Device->CreateRasterizerState(&Desc, &RasterizerStateCullBack)))
    {
        return false;
    }

    Desc.CullMode = D3D11_CULL_FRONT;
    if (FAILED(Device->CreateRasterizerState(&Desc, &RasterizerStateCullFront)))
    {
        return false;
    }

    Desc.CullMode = D3D11_CULL_NONE;
    if (FAILED(Device->CreateRasterizerState(&Desc, &RasterizerStateCullNone)))
    {
        return false;
    }

    Desc.DepthBias = 16;
    Desc.SlopeScaledDepthBias = 1.0f;
    Desc.DepthBiasClamp = 0.0f;

    if (FAILED(Device->CreateRasterizerState(&Desc, &RasterizerStateCullNoneDepthBiased)))
    {
        return false;
    }

    return true;
}

void URenderer::ReleaseRasterizerStates()
{
    if (RasterizerStateCullBack)
    {
        RasterizerStateCullBack->Release();
        RasterizerStateCullBack = nullptr;
    }

    if (RasterizerStateCullFront)
    {
        RasterizerStateCullFront->Release();
        RasterizerStateCullFront = nullptr;
    }

    if (RasterizerStateCullNone)
    {
        RasterizerStateCullNone->Release();
        RasterizerStateCullNone = nullptr;
    }

    if (RasterizerStateCullNoneDepthBiased)
    {
        RasterizerStateCullNoneDepthBiased->Release();
        RasterizerStateCullNoneDepthBiased = nullptr;
    }
}

bool URenderer::CreateDepthStencilStates()
{
    if (!Device)
    {
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC Desc = {};
    Desc.DepthEnable = TRUE;
    Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    Desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

    if (FAILED(Device->CreateDepthStencilState(&Desc, &DepthStencilStateEnabled)))
    {
        return false;
    }

    Desc.DepthEnable = FALSE;
    Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    Desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

    if (FAILED(Device->CreateDepthStencilState(&Desc, &DepthStencilStateDisabled)))
    {
        return false;
    }

    // 추가: depth test만 하고 write는 안 함
    Desc.DepthEnable = TRUE;
    Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    Desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

    if (FAILED(Device->CreateDepthStencilState(&Desc, &DepthStencilStateTestOnly)))
    {
        return false;
    }


    return true;
}

void URenderer::ReleaseDepthStencilStates()
{
    if (DepthStencilStateEnabled)
    {
        DepthStencilStateEnabled->Release();
        DepthStencilStateEnabled = nullptr;
    }

    if (DepthStencilStateDisabled)
    {
        DepthStencilStateDisabled->Release();
        DepthStencilStateDisabled = nullptr;
    }
    if (DepthStencilStateTestOnly)
    {
        DepthStencilStateTestOnly->Release();
        DepthStencilStateTestOnly = nullptr;
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
        SimpleVertexShader->Release();
        SimpleVertexShader = nullptr;

        return false;
    }

    D3D11_INPUT_ELEMENT_DESC Layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    Hr = Device->CreateInputLayout(Layout, ARRAYSIZE(Layout), VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), &SimpleInputLayout);
    if (FAILED(Hr))
    {
        VSBlob->Release();
        PSBlob->Release();
        SimpleVertexShader->Release();
        SimpleVertexShader = nullptr;
        SimplePixelShader->Release();
        SimplePixelShader = nullptr;

        return false;
    }

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
    DeviceContext->RSSetState(RasterizerStateCullBack);
    DeviceContext->OMSetDepthStencilState(DepthStencilStateEnabled, 0);

    DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, DepthStencilView);

    if (ConstantBuffer)
    {
        DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
    }
}

void URenderer::UpdateVSConstants(const FMatrix& World, const FMatrix& View, const FMatrix& Projection, const FVector4& Color, bool bUseVertexColor)
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
    Constants->bUseVertexColor = bUseVertexColor ? 1u : 0u; // @@@ 1u, 0u가 뭐지

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
        //View = FMatrix::MakeViewMatrix(
        //    Camera->GetRelativeLocation(),
        //    Camera->GetRelativeRotation());

        //Projection = FMatrix::MakePerspectiveMatrix(
        //    Camera->GetFieldOfView(),
        //    Camera->GetAspectRatio(),
        //    Camera->GetNearClip(),
        //    Camera->GetFarClip());

        //printf("Camera Aspect: %f\n", Camera->GetAspectRatio());
        //printf("Viewport: %f x %f\n", ViewportInfo.Width, ViewportInfo.Height);
        //printf("Viewport Aspect: %f\n", ViewportInfo.Width / ViewportInfo.Height);

        View = Camera->GetViewMatrix();
        Projection = Camera->GetProjectionMatrix();

        //printf("Proj[0][0] = %f\n", Projection.M[0][0]);
        //printf("Proj[1][1] = %f\n", Projection.M[1][1]);
        //printf("Proj[2][2] = %f\n", Projection.M[2][2]);
        //printf("Proj[2][3] = %f\n", Projection.M[2][3]);
        //printf("Proj[3][2] = %f\n", Projection.M[3][2]);
        //printf("Proj[3][3] = %f\n", Projection.M[3][3]);
    }

    //for (const FRenderItem& Item : Scene.RenderItems)
    //{
    //    DrawMeshItem(Item, View, Projection);
    //}

    // 1) 일반 오브젝트 먼저
    for (const FRenderItem& Item : Scene.RenderItems)
    {
        if (!Item.bDepthEnable)
        {
            continue;
        }

        DrawMeshItem(Item, View, Projection);
    }

    ClearDepthOnly();

    //// 2) overlay 성격 오브젝트 나중
    //for (const FRenderItem& Item : Scene.RenderItems)
    //{
    //    if (Item.bDepthEnable)
    //    {
    //        continue;
    //    }

    //    DrawMeshItem(Item, View, Projection);
    //}
    // 2차: gizmo만
    for (const FRenderItem& Item : Scene.RenderItems)
    {
        if (!Item.bIsGizmo) continue;
        DrawMeshItem(Item, View, Projection);
    }

    RenderHitProxy(Scene, Camera);
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

    // for Hit Proxy
    ReleaseHitProxyBuffer();
    CreateHitProxyBuffer(Width, Height);
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

    ID3D11RasterizerState* TargetRS = RasterizerStateCullBack;
    if (Item.bUseDepthBias)
    {
        TargetRS = RasterizerStateCullNoneDepthBiased;
    }
    else
    {
        switch (Item.CullMode)
        {
        case ERenderCullMode::Back:
            TargetRS = RasterizerStateCullBack;
            break;
        case ERenderCullMode::Front:
            TargetRS = RasterizerStateCullFront;
            break;
        case ERenderCullMode::None:
            TargetRS = RasterizerStateCullNone;
            break;
        }
    }
    DeviceContext->RSSetState(TargetRS);
    DeviceContext->OMSetDepthStencilState(
        Item.bDepthEnable ? DepthStencilStateEnabled : DepthStencilStateDisabled,
        0);

    UpdateVSConstants(Item.WorldMatrix, View, Projection, Item.Color, Item.bUseVertexColor);

    UINT Offset = 0;
    DeviceContext->IASetVertexBuffers(0, 1, &Resource.VertexBuffer, &Stride, &Offset);

    if (Resource.IndexBuffer && Resource.IndexCount > 0)
    {
        DeviceContext->IASetIndexBuffer(Resource.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    }
    else
    {
        DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    }

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

    ID3D11DepthStencilState* DepthState = DepthStencilStateDisabled;

    if (Item.bDepthEnable)
    {
        DepthState = Item.bDepthWrite ? DepthStencilStateEnabled : DepthStencilStateTestOnly;
    }

    DeviceContext->OMSetDepthStencilState(DepthState, 0);
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

FHitProxy URenderer::PickPrimitiveProxy(int MouseX, int MouseY)
{
    if (!DeviceContext || !HitProxyTexture || !HitProxyReadbackTexture)
    {
        return FHitProxy{};
    }

    if (MouseX < 0 || MouseY < 0)
    {
        return FHitProxy{};
    }

    D3D11_TEXTURE2D_DESC Desc = {};
    HitProxyTexture->GetDesc(&Desc);

    // 마우스 입력 유효 검사
    if (MouseX >= static_cast<int>(Desc.Width) || MouseY >= static_cast<int>(Desc.Height))
    {
        return FHitProxy{};
    }

    // GPU to CPU 텍스처 복사
    DeviceContext->CopyResource(HitProxyReadbackTexture, HitProxyTexture);

    D3D11_MAPPED_SUBRESOURCE MappedResource = {};
    HRESULT Hr = DeviceContext->Map(HitProxyReadbackTexture, 0, D3D11_MAP_READ, 0, &MappedResource);
    if (FAILED(Hr))
    {
        return FHitProxy{};
    }

    const uint8* Data = static_cast<const uint8*>(MappedResource.pData);
    const uint32 BytesPerPixel = 4;

    // 마우스 클릭 위치에 대응하는 메모리 상 지점
    const uint8* Pixel = Data + MouseY * MappedResource.RowPitch + MouseX * BytesPerPixel;

    const uint8 R = Pixel[0];
    const uint8 G = Pixel[1];
    const uint8 B = Pixel[2];
    const uint8 A = Pixel[3];

    DeviceContext->Unmap(HitProxyReadbackTexture, 0);

    if (A == 0 && R == 0 && G == 0 && B == 0)
    {
        // ID = 0 = 배경
        return FHitProxy{};
    }

    const uint32 ProxyId = DecodeHitProxyIdColor(R, G, B);
    if (ProxyId == 0)
    {
        return FHitProxy{};
    }

    auto It = HitProxyMap.find(ProxyId);
    if (It == HitProxyMap.end())
    {
        return FHitProxy{};
    }

    return It->second;
}

bool URenderer::CreateHitProxyBuffer(UINT Width, UINT Height)
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
    Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    Desc.SampleDesc.Count = 1;
    Desc.Usage = D3D11_USAGE_DEFAULT;
    Desc.BindFlags = D3D11_BIND_RENDER_TARGET;

    HRESULT Hr = Device->CreateTexture2D(&Desc, nullptr, &HitProxyTexture);
    if (FAILED(Hr))
    {
        return false;
    }

    Hr = Device->CreateRenderTargetView(HitProxyTexture, nullptr, &HitProxyRTV);
    if (FAILED(Hr))
    {
        HitProxyTexture->Release();
        HitProxyTexture = nullptr;

        return false;
    }

    D3D11_TEXTURE2D_DESC ReadbackDesc = Desc;
    ReadbackDesc.Usage = D3D11_USAGE_STAGING;
    ReadbackDesc.BindFlags = 0;
    ReadbackDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    Hr = Device->CreateTexture2D(&ReadbackDesc, nullptr, &HitProxyReadbackTexture);
    if (FAILED(Hr))
    {
        HitProxyRTV->Release();
        HitProxyRTV = nullptr;

        HitProxyTexture->Release();
        HitProxyTexture = nullptr;

        return false;
    }

    return true;
}

void URenderer::ReleaseHitProxyBuffer()
{
    if (HitProxyReadbackTexture)
    {
        HitProxyReadbackTexture->Release();
        
        HitProxyReadbackTexture = nullptr;
    }

    if (HitProxyRTV)
    {
        HitProxyRTV->Release();

        HitProxyRTV = nullptr;
    }

    if (HitProxyTexture)
    {
        HitProxyTexture->Release();

        HitProxyTexture = nullptr;
    }
}

bool URenderer::CreateHitProxyShader(const wchar_t* ShaderPath)
{
    if (!Device)
    {
        return false;
    }

    ID3DBlob* PSBlob = CompileShaderOrNull(ShaderPath, "mainHPPS", "ps_5_0");
    if (!PSBlob)
    {
        return false;
    }

    HRESULT Hr = Device->CreatePixelShader(PSBlob->GetBufferPointer(), PSBlob->GetBufferSize(), nullptr, &HitProxyPixelShader);
    if (FAILED(Hr))
    {
        PSBlob->Release();

        return false;
    }

    PSBlob->Release();

    return SUCCEEDED(Hr);
}

void URenderer::ReleaseHitProxyShader()
{
    if (HitProxyPixelShader)
    {
        HitProxyPixelShader->Release();
        HitProxyPixelShader = nullptr;
    }
}

void URenderer::PrepareHitProxyPipeline()
{
    if (!DeviceContext || !HitProxyRTV || !DepthStencilView)
    {
        return;
    }

    const FLOAT HitProxyClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    DeviceContext->ClearRenderTargetView(HitProxyRTV, HitProxyClearColor);
    DeviceContext->ClearDepthStencilView(
        DepthStencilView,
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
        1.0f,
        0
    );

    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    DeviceContext->IASetInputLayout(SimpleInputLayout);

    DeviceContext->VSSetShader(SimpleVertexShader, nullptr, 0);
    DeviceContext->PSSetShader(HitProxyPixelShader, nullptr, 0);

    DeviceContext->RSSetViewports(1, &ViewportInfo);
    //DeviceContext->RSSetState(RasterizerState);
    DeviceContext->RSSetState(RasterizerStateCullBack);
    DeviceContext->OMSetDepthStencilState(DepthStencilStateEnabled, 0);

    DeviceContext->OMSetRenderTargets(1, &HitProxyRTV, DepthStencilView);

    if (ConstantBuffer)
    {
        DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
        DeviceContext->PSSetConstantBuffers(0, 1, &ConstantBuffer);
    }
}

void URenderer::RenderHitProxy(const FScene& Scene, const UCameraComponent* Camera)
{
    if (!Device || !DeviceContext || !HitProxyRTV)
    {
        return;
    }

    // Id - 아이템 맵 초기화
    HitProxyMap.clear();

    FMatrix View = FMatrix::Identity;
    FMatrix Projection = FMatrix::Identity;

    if (Camera)
    {
        View = Camera->GetViewMatrix();
        Projection = Camera->GetProjectionMatrix();
    }

    /* 일반 렌더링 파이프라인에서는 BeginFrame()에서 불러서 먼저 사용하지만, 
       일반 렌더링 파이프라인이 끝난 후 Hit Proxy 파이프라인을 태워야하기 때문에 여기서 호출 */
    PrepareHitProxyPipeline();

    uint32 NextProxyId = 1;

    // 1) 일반 오브젝트 먼저
    for (const FRenderItem& Item : Scene.RenderItems)
    {
        if (Item.HitProxy.Type == EHitProxyType::None)
        {
            continue;
        }

        if (!Item.bDepthEnable)
        {
            continue;
        }

        HitProxyMap[NextProxyId] = Item.HitProxy;
        DrawHitProxyItem(Item, View, Projection, NextProxyId);
        NextProxyId++;
    }

    // 2) Overlay 성격 오브젝트 나중
    for (const FRenderItem& Item : Scene.RenderItems)
    {
        if (Item.HitProxy.Type == EHitProxyType::None)
        {
            continue;
        }

        if (Item.bDepthEnable)
        {
            continue;
        }

        HitProxyMap[NextProxyId] = Item.HitProxy;
        DrawHitProxyItem(Item, View, Projection, NextProxyId);
        NextProxyId++;
    }
}

void URenderer::DrawHitProxyItem(const FRenderItem& Item, const FMatrix& View, const FMatrix& Projection, uint32 ProxyId)
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

    ID3D11RasterizerState* TargetRS = RasterizerStateCullBack;
    switch (Item.CullMode)
    {
    case ERenderCullMode::Back:
        TargetRS = RasterizerStateCullBack;
        break;
    case ERenderCullMode::Front:
        TargetRS = RasterizerStateCullFront;
        break;
    case ERenderCullMode::None:
        TargetRS = RasterizerStateCullNone;
        break;
    }

    DeviceContext->RSSetState(TargetRS);
    DeviceContext->OMSetDepthStencilState(
        Item.bDepthEnable ? DepthStencilStateEnabled : DepthStencilStateDisabled,
        0);

    // Id를 인코딩하여 색으로 사용
    const FVector4 ProxyColor = EncodeHitProxyIdColor(ProxyId);
    UpdateVSConstants(Item.WorldMatrix, View, Projection, ProxyColor, false);

    UINT Offset = 0;
    DeviceContext->IASetVertexBuffers(0, 1, &Resource.VertexBuffer, &Stride, &Offset);
    DeviceContext->IASetPrimitiveTopology(Resource.Topology);

    if (Resource.IndexBuffer && Resource.IndexCount > 0)
    {
        DeviceContext->IASetIndexBuffer(Resource.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
        //DeviceContext->DrawIndexed(Resource.IndexCount, 0, 0);
    }
    else
    {
        //DeviceContext->Draw(Resource.VertexCount, 0);
        DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    }

    DeviceContext->IASetPrimitiveTopology(Resource.Topology);

    if (Resource.IndexBuffer && Resource.IndexCount > 0)
    {
        DeviceContext->DrawIndexed(Resource.IndexCount, 0, 0);
    }
    else
    {
        DeviceContext->Draw(Resource.VertexCount, 0);
    }
}

FVector4 URenderer::EncodeHitProxyIdColor(uint32 Id)
{
    // Id의 하위 24bit을 색으로 인코딩하여 사용
    // Id == 0은 배경으로 약속
    uint8 R = (Id & 0x000000FF);
    uint8 G = (Id & 0x0000FF00) >> 8;
    uint8 B = (Id & 0x00FF0000) >> 16;

    return FVector4(R / 255.0f, G / 255.0f, B / 255.0f, 1.0f);
}

uint32 URenderer::DecodeHitProxyIdColor(uint8 R, uint8 G, uint8 B)
{
    // 하위 24bit 이어 붙이기
    return (uint32) R | ((uint32) G << 8) | ((uint32) B << 16);
}

void URenderer::BindMainRenderTargetForOverlay()
{
    if (!DeviceContext || !FrameBufferRTV || !DepthStencilView)
    {
        return;
    }

    DeviceContext->RSSetViewports(1, &ViewportInfo);
    DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, DepthStencilView);
}

void URenderer::ClearDepthOnly()
{
    if (DeviceContext && DepthStencilView)
    {
        DeviceContext->ClearDepthStencilView(
            DepthStencilView,
            D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
            1.0f,
            0
        );
    }
}