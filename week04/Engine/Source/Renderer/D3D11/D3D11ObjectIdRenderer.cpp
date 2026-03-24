#include "Renderer/D3D11/D3D11ObjectIdRenderer.h"

#include "Renderer/D3D11/D3D11RHI.h"
#include "Renderer/SceneView.h"
#include "Resources/Mesh/Cone.h"
#include "Resources/Mesh/Cube.h"
#include "Resources/Mesh/Cylinder.h"
#include "Resources/Mesh/Quad.h"
#include "Resources/Mesh/Ring.h"
#include "Resources/Mesh/Sphere.h"
#include "Resources/Mesh/Triangle.h"
#include "Resources/Mesh/VertexSimple.h"

namespace
{
    struct alignas(16) FObjectIdConstants
    {
        FMatrix MVP;
        uint32  ObjectId = 0;
        uint32  Padding0 = 0;
        uint32  Padding1 = 0;
        uint32  Padding2 = 0;
    };
} // namespace

bool FD3D11ObjectIdRenderer::Initialize(FD3D11RHI* InRHI)
{
    if (InRHI == nullptr)
    {
        return false;
    }

    RHI = InRHI;

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

    if (!CreateBasicMeshes())
    {
        Shutdown();
        return false;
    }

    if (!CreatePickResources(RHI->GetViewportWidth(), RHI->GetViewportHeight()))
    {
        Shutdown();
        return false;
    }

    return true;
}

void FD3D11ObjectIdRenderer::Shutdown()
{
    RenderItems.clear();

    ReleasePickResources();
    ReleaseBasicMeshes();

    DepthStencilState.Reset();
    RasterizerState.Reset();

    ConstantBuffer.Reset();
    InputLayout.Reset();
    PixelShader.Reset();
    VertexShader.Reset();

    CurrentSceneView = nullptr;
    MouseX = -1;
    MouseY = -1;

    TargetWidth = 0;
    TargetHeight = 0;
    RHI = nullptr;
}

bool FD3D11ObjectIdRenderer::Resize(int32 InWidth, int32 InHeight)
{
    if (RHI == nullptr || InWidth <= 0 || InHeight <= 0)
    {
        return false;
    }

    if (TargetWidth == InWidth && TargetHeight == InHeight)
    {
        return true;
    }

    ReleasePickResources();
    return CreatePickResources(InWidth, InHeight);
}

void FD3D11ObjectIdRenderer::BeginFrame(const FSceneView* InSceneView, int32 InMouseX,
                                        int32 InMouseY)
{
    CurrentSceneView = InSceneView;
    MouseX = InMouseX;
    MouseY = InMouseY;
    RenderItems.clear();
}

void FD3D11ObjectIdRenderer::AddPrimitive(const FObjectIdRenderItem& InItem)
{
    if (InItem.ObjectId == 0)
    {
        return;
    }

    RenderItems.push_back(InItem);
}

void FD3D11ObjectIdRenderer::AddPrimitives(const TArray<FObjectIdRenderItem>& InItems)
{
    for (const FObjectIdRenderItem& Item : InItems)
    {
        AddPrimitive(Item);
    }
}

bool FD3D11ObjectIdRenderer::RenderAndReadBack(uint32& OutPickId)
{
    OutPickId = 0;

    if (RHI == nullptr || CurrentSceneView == nullptr)
    {
        RenderItems.clear();
        CurrentSceneView = nullptr;
        return false;
    }

    if (MouseX < 0 || MouseY < 0 || MouseX >= TargetWidth || MouseY >= TargetHeight)
    {
        RenderItems.clear();
        CurrentSceneView = nullptr;
        return false;
    }

    if (RHI->GetDeviceContext() == nullptr || PickRTV == nullptr || PickDSV == nullptr)
    {
        RenderItems.clear();
        CurrentSceneView = nullptr;
        return false;
    }

    ID3D11RenderTargetView* RTV = PickRTV.Get();
    RHI->SetRenderTargets(1, &RTV, PickDSV.Get());
    RHI->SetViewport(RHI->GetViewport());

    static const float ClearColor[4] = {0, 0, 0, 0};
    RHI->ClearRenderTarget(PickRTV.Get(), ClearColor);
    RHI->ClearDepthStencil(PickDSV.Get(), 1.0f, 0);

    BindPipeline();

    for (const FObjectIdRenderItem& Item : RenderItems)
    {
        FObjectIdMeshResource* MeshResource = GetBasicMeshResource(Item.MeshType);
        if (MeshResource == nullptr || MeshResource->VertexBuffer == nullptr ||
            MeshResource->IndexBuffer == nullptr || MeshResource->IndexCount == 0)
        {
            continue;
        }

        BindPrimitiveTopology(MeshResource->Topology);

        const UINT    Stride = sizeof(FVertexSimple);
        const UINT    Offset = 0;
        ID3D11Buffer* VB = MeshResource->VertexBuffer.Get();

        RHI->SetVertexBuffer(0, VB, Stride, Offset);
        RHI->SetIndexBuffer(MeshResource->IndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

        FObjectIdConstants Constants = {};
        Constants.MVP = Item.World * CurrentSceneView->GetViewProjectionMatrix();
        Constants.ObjectId = Item.ObjectId;

        if (!RHI->UpdateConstantBuffer(ConstantBuffer.Get(), &Constants, sizeof(Constants)))
        {
            continue;
        }

        RHI->DrawIndexed(MeshResource->IndexCount, 0, 0);
    }

    const bool bReadOk = ReadBackMousePixel(OutPickId);

    RHI->SetDefaultRenderTargets();

    RenderItems.clear();
    CurrentSceneView = nullptr;

    return bReadOk;
}

bool FD3D11ObjectIdRenderer::CreateShaders()
{
    if (RHI == nullptr)
    {
        return false;
    }

    static const D3D11_INPUT_ELEMENT_DESC InputElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    if (!RHI->CreateVertexShaderAndInputLayout(
            ShaderPath, "VSMain", InputElements, static_cast<uint32>(std::size(InputElements)),
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

bool FD3D11ObjectIdRenderer::CreateConstantBuffer()
{
    return (RHI != nullptr) &&
           RHI->CreateConstantBuffer(sizeof(FObjectIdConstants), ConstantBuffer.GetAddressOf());
}

bool FD3D11ObjectIdRenderer::CreateStates()
{
    if (RHI == nullptr || RHI->GetDevice() == nullptr)
    {
        return false;
    }

    ID3D11Device* Device = RHI->GetDevice();

    {
        D3D11_RASTERIZER_DESC Desc = {};
        Desc.FillMode = D3D11_FILL_SOLID;
        Desc.CullMode = D3D11_CULL_BACK;
        Desc.FrontCounterClockwise = FALSE;
        Desc.DepthClipEnable = TRUE;

        if (FAILED(Device->CreateRasterizerState(&Desc, RasterizerState.GetAddressOf())))
        {
            return false;
        }
    }

    {
        D3D11_DEPTH_STENCIL_DESC Desc = {};
        Desc.DepthEnable = TRUE;
        Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        Desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

        if (FAILED(Device->CreateDepthStencilState(&Desc, DepthStencilState.GetAddressOf())))
        {
            return false;
        }
    }

    return true;
}

bool FD3D11ObjectIdRenderer::CreatePickResources(int32 InWidth, int32 InHeight)
{
    if (RHI == nullptr || RHI->GetDevice() == nullptr || InWidth <= 0 || InHeight <= 0)
    {
        return false;
    }

    ID3D11Device* Device = RHI->GetDevice();

    {
        D3D11_TEXTURE2D_DESC Desc = {};
        Desc.Width = static_cast<UINT>(InWidth);
        Desc.Height = static_cast<UINT>(InHeight);
        Desc.MipLevels = 1;
        Desc.ArraySize = 1;
        Desc.Format = DXGI_FORMAT_R32_UINT;
        Desc.SampleDesc.Count = 1;
        Desc.Usage = D3D11_USAGE_DEFAULT;
        Desc.BindFlags = D3D11_BIND_RENDER_TARGET;

        if (FAILED(Device->CreateTexture2D(&Desc, nullptr, PickColorTexture.GetAddressOf())))
        {
            return false;
        }

        if (FAILED(Device->CreateRenderTargetView(PickColorTexture.Get(), nullptr,
                                                  PickRTV.GetAddressOf())))
        {
            ReleasePickResources();
            return false;
        }
    }

    {
        D3D11_TEXTURE2D_DESC Desc = {};
        Desc.Width = static_cast<UINT>(InWidth);
        Desc.Height = static_cast<UINT>(InHeight);
        Desc.MipLevels = 1;
        Desc.ArraySize = 1;
        Desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        Desc.SampleDesc.Count = 1;
        Desc.Usage = D3D11_USAGE_DEFAULT;
        Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

        if (FAILED(Device->CreateTexture2D(&Desc, nullptr, PickDepthTexture.GetAddressOf())))
        {
            ReleasePickResources();
            return false;
        }

        if (FAILED(Device->CreateDepthStencilView(PickDepthTexture.Get(), nullptr,
                                                  PickDSV.GetAddressOf())))
        {
            ReleasePickResources();
            return false;
        }
    }

    {
        D3D11_TEXTURE2D_DESC Desc = {};
        Desc.Width = 1;
        Desc.Height = 1;
        Desc.MipLevels = 1;
        Desc.ArraySize = 1;
        Desc.Format = DXGI_FORMAT_R32_UINT;
        Desc.SampleDesc.Count = 1;
        Desc.Usage = D3D11_USAGE_STAGING;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        if (FAILED(Device->CreateTexture2D(&Desc, nullptr, ReadbackTexture.GetAddressOf())))
        {
            ReleasePickResources();
            return false;
        }
    }

    TargetWidth = InWidth;
    TargetHeight = InHeight;
    return true;
}

void FD3D11ObjectIdRenderer::ReleasePickResources()
{
    ReadbackTexture.Reset();

    PickDSV.Reset();
    PickDepthTexture.Reset();

    PickRTV.Reset();
    PickColorTexture.Reset();

    TargetWidth = 0;
    TargetHeight = 0;
}

bool FD3D11ObjectIdRenderer::CreateBasicMeshes()
{
    bool bOk = true;

    bOk &= CreateBasicSphereMesh(MeshResources[static_cast<int32>(EBasicMeshType::Sphere)]);
    bOk &= CreateBasicCubeMesh(MeshResources[static_cast<int32>(EBasicMeshType::Cube)]);
    bOk &= CreateBasicTriangleMesh(MeshResources[static_cast<int32>(EBasicMeshType::Triangle)]);
    bOk &= CreateBasicQuadMesh(MeshResources[static_cast<int32>(EBasicMeshType::Quad)]);
    bOk &= CreateBasicConeMesh(MeshResources[static_cast<int32>(EBasicMeshType::Cone)]);
    bOk &= CreateBasicCylinderMesh(MeshResources[static_cast<int32>(EBasicMeshType::Cylinder)]);
    bOk &= CreateBasicRingMesh(MeshResources[static_cast<int32>(EBasicMeshType::Ring)]);

    return bOk;
}

void FD3D11ObjectIdRenderer::ReleaseBasicMeshes()
{
    for (int32 i = 0; i < static_cast<int32>(EBasicMeshType::Count); ++i)
    {
        MeshResources[i].VertexBuffer.Reset();
        MeshResources[i].IndexBuffer.Reset();
        MeshResources[i].IndexCount = 0;
        MeshResources[i].Topology = EMeshPrimitiveTopology::TriangleList;
    }
}

bool FD3D11ObjectIdRenderer::CreateBasicMeshResource(const FVertexSimple* InVertices,
                                                     uint32 InVertexCount, const uint16* InIndices,
                                                     uint32                 InIndexCount,
                                                     EMeshPrimitiveTopology InTopology,
                                                     FObjectIdMeshResource& OutResource)
{
    if (RHI == nullptr || InVertices == nullptr || InIndices == nullptr || InVertexCount == 0 ||
        InIndexCount == 0)
    {
        return false;
    }

    if (!RHI->CreateVertexBuffer(InVertices, sizeof(FVertexSimple) * InVertexCount,
                                 sizeof(FVertexSimple), false,
                                 OutResource.VertexBuffer.GetAddressOf()))
    {
        return false;
    }

    if (!RHI->CreateIndexBuffer(InIndices, sizeof(uint16) * InIndexCount, false,
                                OutResource.IndexBuffer.GetAddressOf()))
    {
        OutResource.VertexBuffer.Reset();
        return false;
    }

    OutResource.IndexCount = InIndexCount;
    OutResource.Topology = InTopology;
    return true;
}

bool FD3D11ObjectIdRenderer::CreateBasicCubeMesh(FObjectIdMeshResource& OutResource)
{
    return CreateBasicMeshResource(cube_vertices, cube_vertex_count, cube_indices, cube_index_count,
                                   cube_topology, OutResource);
}

bool FD3D11ObjectIdRenderer::CreateBasicQuadMesh(FObjectIdMeshResource& OutResource)
{
    return CreateBasicMeshResource(quad_vertices, quad_vertex_count, quad_indices, quad_index_count,
                                   quad_topology, OutResource);
}

bool FD3D11ObjectIdRenderer::CreateBasicTriangleMesh(FObjectIdMeshResource& OutResource)
{
    return CreateBasicMeshResource(triangle_vertices, triangle_vertex_count, triangle_indices,
                                   triangle_index_count, triangle_topology, OutResource);
}

bool FD3D11ObjectIdRenderer::CreateBasicSphereMesh(FObjectIdMeshResource& OutResource)
{
    return CreateBasicMeshResource(sphere_vertices, sphere_vertex_count, sphere_indices,
                                   sphere_index_count, sphere_topology, OutResource);
}

bool FD3D11ObjectIdRenderer::CreateBasicConeMesh(FObjectIdMeshResource& OutResource)
{
    return CreateBasicMeshResource(cone_vertices, cone_vertex_count, cone_indices, cone_index_count,
                                   cone_topology, OutResource);
}

bool FD3D11ObjectIdRenderer::CreateBasicCylinderMesh(FObjectIdMeshResource& OutResource)
{
    return CreateBasicMeshResource(cylinder_vertices, cylinder_vertex_count, cylinder_indices,
                                   cylinder_index_count, cylinder_topology, OutResource);
}

bool FD3D11ObjectIdRenderer::CreateBasicRingMesh(FObjectIdMeshResource& OutResource)
{
    return CreateBasicMeshResource(ring_vertices, ring_vertex_count, ring_indices, ring_index_count,
                                   ring_topology, OutResource);
}

FObjectIdMeshResource* FD3D11ObjectIdRenderer::GetBasicMeshResource(EBasicMeshType InType)
{
    const int32 Index = static_cast<int32>(InType);
    if (Index < 0 || Index >= static_cast<int32>(EBasicMeshType::Count))
    {
        return nullptr;
    }

    return &MeshResources[Index];
}

void FD3D11ObjectIdRenderer::BindPipeline()
{
    if (RHI == nullptr)
    {
        return;
    }

    RHI->SetInputLayout(InputLayout.Get());
    RHI->SetVertexShader(VertexShader.Get());
    RHI->SetPixelShader(PixelShader.Get());
    RHI->SetVSConstantBuffer(0, ConstantBuffer.Get());
    RHI->SetPSConstantBuffer(0, ConstantBuffer.Get());

    RHI->SetRasterizerState(RasterizerState.Get());
    RHI->SetDepthStencilState(DepthStencilState.Get(), 0);
}

void FD3D11ObjectIdRenderer::BindPrimitiveTopology(EMeshPrimitiveTopology InTopology)
{
    if (RHI == nullptr)
    {
        return;
    }

    switch (InTopology)
    {
    case EMeshPrimitiveTopology::TriangleList:
        RHI->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        break;
    case EMeshPrimitiveTopology::TriangleStrip:
        RHI->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        break;
    case EMeshPrimitiveTopology::LineList:
        RHI->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        break;
    case EMeshPrimitiveTopology::LineStrip:
        RHI->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
        break;
    default:
        RHI->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        break;
    }
}

bool FD3D11ObjectIdRenderer::ReadBackMousePixel(uint32& OutObjectId)
{
    if (RHI == nullptr || RHI->GetDeviceContext() == nullptr || PickColorTexture == nullptr ||
        ReadbackTexture == nullptr)
    {
        return false;
    }

    ID3D11DeviceContext* Context = RHI->GetDeviceContext();

    D3D11_BOX Box = {};
    Box.left = static_cast<UINT>(MouseX);
    Box.right = static_cast<UINT>(MouseX + 1);
    Box.top = static_cast<UINT>(MouseY);
    Box.bottom = static_cast<UINT>(MouseY + 1);
    Box.front = 0;
    Box.back = 1;

    Context->CopySubresourceRegion(ReadbackTexture.Get(), 0, 0, 0, 0, PickColorTexture.Get(), 0,
                                   &Box);

    D3D11_MAPPED_SUBRESOURCE Mapped = {};
    if (FAILED(Context->Map(ReadbackTexture.Get(), 0, D3D11_MAP_READ, 0, &Mapped)))
    {
        return false;
    }

    OutObjectId = *reinterpret_cast<const uint32*>(Mapped.pData);
    Context->Unmap(ReadbackTexture.Get(), 0);
    return true;
}