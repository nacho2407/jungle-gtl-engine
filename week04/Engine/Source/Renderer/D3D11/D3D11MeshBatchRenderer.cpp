#include <cstring>
#include "Core/Math/Matrix.h"

#include "Renderer/D3D11/D3D11MeshBatchRenderer.h"
#include "Renderer/D3D11/D3D11DynamicRHI.h"
#include "Renderer/Types/ShaderConstants.h"

#include "Renderer/SceneRenderData.h"
#include "Renderer/SceneView.h"
#include "Renderer/Types/ViewMode.h"

#include "Resources/Mesh/VertexSimple.h"
#include "Resources/Mesh/Sphere.h"
#include "Resources/Mesh/Triangle.h"
#include "Resources/Mesh/Cube.h"
#include "Resources/Mesh/Plane.h"

namespace
{
    inline uint32 ToMeshIndex(EBasicMeshType InType) { return static_cast<uint32>(InType); }

    inline uint32 Align16(uint32 InValue) { return (InValue + 15u) & ~15u; }

    static bool UploadInstanceRange(ID3D11DeviceContext* InContext, ID3D11Buffer* InInstanceBuffer,
                                    const FMeshInstanceData* InInstanceData, uint32 InInstanceCount)
    {
        if (InContext == nullptr || InInstanceBuffer == nullptr || InInstanceData == nullptr ||
            InInstanceCount == 0)
        {
            return false;
        }

        D3D11_MAPPED_SUBRESOURCE Mapped = {};
        if (FAILED(InContext->Map(InInstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
        {
            return false;
        }

        std::memcpy(Mapped.pData, InInstanceData, sizeof(FMeshInstanceData) * InInstanceCount);
        InContext->Unmap(InInstanceBuffer, 0);
        return true;
    }
} // namespace

bool FD3D11MeshBatchRenderer::Initialize(FD3D11DynamicRHI* InRHI)
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

    if (!CreateConstantBuffers())
    {
        Shutdown();
        return false;
    }

    if (!CreateStates())
    {
        Shutdown();
        return false;
    }

    if (!CreateDynamicInstanceBuffer(MaxInstanceCount))
    {
        Shutdown();
        return false;
    }

    if (!CreateBasicMeshes())
    {
        Shutdown();
        return false;
    }

    ResetBatches();
    return true;
}

void FD3D11MeshBatchRenderer::Shutdown()
{
    ResetBatches();
    ReleaseBasicMeshes();

    DepthStencilState.Reset();
    WireframeRasterizerState.Reset();
    SolidRasterizerState.Reset();

    InstanceBuffer.Reset();
    ConstantBuffer.Reset();
    InputLayout.Reset();
    PixelShader.Reset();
    VertexShader.Reset();

    ViewMode = EViewModeIndex::Lit;
    RHI = nullptr;
}

void FD3D11MeshBatchRenderer::Render(const FSceneRenderData& InRenderData)
{
    if (RHI == nullptr || InRenderData.SceneView == nullptr)
    {
        return;
    }

    ViewMode = InRenderData.ViewMode;

    ResetBatches();
    GatherRenderItems(InRenderData);
    Flush(InRenderData.SceneView);
}

bool FD3D11MeshBatchRenderer::CreateShaders()
{
    if (RHI == nullptr)
    {
        return false;
    }

    const D3D11_INPUT_ELEMENT_DESC InputElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},

        {"INSTANCE_WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA,
         1},
        {"INSTANCE_WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA,
         1},
        {"INSTANCE_WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA,
         1},
        {"INSTANCE_WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA,
         1},

        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    };

    if (!RHI->CreateVertexShaderAndInputLayout(DefaultShaderPath, "VSMain", InputElements,
                                               static_cast<uint32>(std::size(InputElements)),
                                               VertexShader.GetAddressOf(),
                                               InputLayout.GetAddressOf()))
    {
        return false;
    }

    if (!RHI->CreatePixelShader(DefaultShaderPath, "PSMain", PixelShader.GetAddressOf()))
    {
        return false;
    }

    return true;
}

bool FD3D11MeshBatchRenderer::CreateConstantBuffers()
{
    if (RHI == nullptr)
    {
        return false;
    }

    return RHI->CreateConstantBuffer(Align16(static_cast<uint32>(sizeof(FMeshUnlitInstancedConstants))),
                                     ConstantBuffer.GetAddressOf());
}

bool FD3D11MeshBatchRenderer::CreateStates()
{
    if (RHI == nullptr)
    {
        return false;
    }

    ID3D11Device* Device = RHI->GetDevice();
    if (Device == nullptr)
    {
        return false;
    }

    {
        D3D11_RASTERIZER_DESC Desc = {};
        Desc.FillMode = D3D11_FILL_SOLID;
        Desc.CullMode = D3D11_CULL_BACK;
        Desc.FrontCounterClockwise = FALSE;
        Desc.DepthClipEnable = TRUE;

        if (FAILED(Device->CreateRasterizerState(&Desc, SolidRasterizerState.GetAddressOf())))
        {
            return false;
        }
    }

    {
        D3D11_RASTERIZER_DESC Desc = {};
        Desc.FillMode = D3D11_FILL_WIREFRAME;
        Desc.CullMode = D3D11_CULL_BACK;
        Desc.FrontCounterClockwise = FALSE;
        Desc.DepthClipEnable = TRUE;

        if (FAILED(Device->CreateRasterizerState(&Desc, WireframeRasterizerState.GetAddressOf())))
        {
            return false;
        }
    }

    {
        D3D11_DEPTH_STENCIL_DESC Desc = {};
        Desc.DepthEnable = TRUE;
        Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        Desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        Desc.StencilEnable = FALSE;

        if (FAILED(Device->CreateDepthStencilState(&Desc, DepthStencilState.GetAddressOf())))
        {
            return false;
        }
    }

    return true;
}

bool FD3D11MeshBatchRenderer::CreateDynamicInstanceBuffer(uint32 InMaxInstanceCount)
{
    if (RHI == nullptr || InMaxInstanceCount == 0)
    {
        return false;
    }

    ID3D11Device* Device = RHI->GetDevice();
    if (Device == nullptr)
    {
        return false;
    }

    D3D11_BUFFER_DESC Desc = {};
    Desc.ByteWidth = sizeof(FMeshInstanceData) * InMaxInstanceCount;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    return SUCCEEDED(Device->CreateBuffer(&Desc, nullptr, InstanceBuffer.GetAddressOf()));
}

bool FD3D11MeshBatchRenderer::CreateBasicMeshes()
{
    if (!CreateBasicSphereMesh(BasicMeshes[ToMeshIndex(EBasicMeshType::Sphere)]))
    {
        return false;
    }

    if (!CreateBasicCubeMesh(BasicMeshes[ToMeshIndex(EBasicMeshType::Cube)]))
    {
        return false;
    }

    if (!CreateBasicTriangleMesh(BasicMeshes[ToMeshIndex(EBasicMeshType::Triangle)]))
    {
        return false;
    }

    if (!CreateBasicPlaneMesh(BasicMeshes[ToMeshIndex(EBasicMeshType::Plane)]))
    {
        return false;
    }

    return true;
}

void FD3D11MeshBatchRenderer::ReleaseBasicMeshes()
{
    for (int32 Index = 0; Index < static_cast<int32>(EBasicMeshType::Count); ++Index)
    {
        BasicMeshes[Index].VertexBuffer.Reset();
        BasicMeshes[Index].IndexBuffer.Reset();
        BasicMeshes[Index].IndexCount = 0;
    }
}

bool FD3D11MeshBatchRenderer::CreateBasicCubeMesh(FBasicMeshResource& OutResource)
{
    return CreateBasicMeshResource(cube_vertices, cube_vertex_count, cube_indices, cube_index_count,
                                   OutResource);
}

bool FD3D11MeshBatchRenderer::CreateBasicPlaneMesh(FBasicMeshResource& OutResource)
{
    return CreateBasicMeshResource(plane_vertices, plane_vertex_count, plane_indices,
                                   plane_index_count, OutResource);
}

bool FD3D11MeshBatchRenderer::CreateBasicTriangleMesh(FBasicMeshResource& OutResource)
{
    return CreateBasicMeshResource(triangle_vertices, triangle_vertex_count, triangle_indices,
                                   triangle_index_count, OutResource);
}

bool FD3D11MeshBatchRenderer::CreateBasicSphereMesh(FBasicMeshResource& OutResource)
{
    return CreateBasicMeshResource(sphere_vertices, sphere_vertex_count, sphere_indices,
                                   sphere_index_count, OutResource);
}

bool FD3D11MeshBatchRenderer::CreateBasicMeshResource(const FVertexSimple* InVertices,
                                                      uint32 InVertexCount, const uint16* InIndices,
                                                      uint32              InIndexCount,
                                                      FBasicMeshResource& OutResource)
{
    if (RHI == nullptr || InVertices == nullptr || InIndices == nullptr || InVertexCount == 0 ||
        InIndexCount == 0)
    {
        return false;
    }

    ID3D11Device* Device = RHI->GetDevice();
    if (Device == nullptr)
    {
        return false;
    }

    TArray<FMeshVertex> MeshVertices;
    MeshVertices.reserve(InVertexCount);

    for (uint32 Index = 0; Index < InVertexCount; ++Index)
    {
        FMeshVertex Vertex = {};
        Vertex.Position = FVector(InVertices[Index].x, InVertices[Index].y, InVertices[Index].z);
        MeshVertices.push_back(Vertex);
    }

    {
        D3D11_BUFFER_DESC Desc = {};
        Desc.ByteWidth = static_cast<uint32>(sizeof(FMeshVertex) * MeshVertices.size());
        Desc.Usage = D3D11_USAGE_IMMUTABLE;
        Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA Data = {};
        Data.pSysMem = MeshVertices.data();

        if (FAILED(Device->CreateBuffer(&Desc, &Data, OutResource.VertexBuffer.GetAddressOf())))
        {
            return false;
        }
    }

    {
        D3D11_BUFFER_DESC Desc = {};
        Desc.ByteWidth = static_cast<uint32>(sizeof(uint16) * InIndexCount);
        Desc.Usage = D3D11_USAGE_IMMUTABLE;
        Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA Data = {};
        Data.pSysMem = InIndices;

        if (FAILED(Device->CreateBuffer(&Desc, &Data, OutResource.IndexBuffer.GetAddressOf())))
        {
            OutResource.VertexBuffer.Reset();
            return false;
        }
    }

    OutResource.IndexCount = InIndexCount;
    return true;
}

void FD3D11MeshBatchRenderer::ResetBatches()
{
    for (int32 Index = 0; Index < static_cast<int32>(EBasicMeshType::Count); ++Index)
    {
        MeshInstances[Index].clear();
    }
}

void FD3D11MeshBatchRenderer::GatherRenderItems(const FSceneRenderData& InRenderData)
{
    for (const FPrimitiveRenderItem& Item : InRenderData.Primitives)
    {
        if (!Item.bVisible)
        {
            continue;
        }

        const uint32 MeshIndex = ToMeshIndex(Item.MeshType);
        if (MeshIndex >= static_cast<uint32>(EBasicMeshType::Count))
        {
            continue;
        }

        FMeshInstanceData Instance = {};
        Instance.World = Item.World;
        Instance.Color = Item.Color;

        if (Item.bSelected)
        {
            Instance.Color.Z *= 0.6f;
        }

        if (Item.bHovered)
        {
            Instance.Color.X *= 1.15f;
            Instance.Color.Y *= 1.15f;
            Instance.Color.Z *= 1.15f;
        }

        MeshInstances[MeshIndex].push_back(Instance);
    }
}

void FD3D11MeshBatchRenderer::Flush(const FSceneView* InSceneView)
{
    if (RHI == nullptr || InSceneView == nullptr)
    {
        return;
    }

    UpdatePerFrameConstants(InSceneView);
    BindPipeline();

    for (int32 Index = 0; Index < static_cast<int32>(EBasicMeshType::Count); ++Index)
    {
        if (MeshInstances[Index].empty())
        {
            continue;
        }

        DrawMeshBatch(static_cast<EBasicMeshType>(Index), InSceneView);
    }
}

void FD3D11MeshBatchRenderer::UpdatePerFrameConstants(const FSceneView* InSceneView)
{
    if (RHI == nullptr || InSceneView == nullptr)
    {
        return;
    }

    ID3D11DeviceContext* Context = RHI->GetDeviceContext();
    if (Context == nullptr)
    {
        return;
    }

    FMeshUnlitInstancedConstants Constants = {};
    Constants.VP = InSceneView->GetViewProjectionMatrix();

    Context->UpdateSubresource(ConstantBuffer.Get(), 0, nullptr, &Constants, 0, 0);
}

void FD3D11MeshBatchRenderer::BindPipeline()
{
    if (RHI == nullptr)
    {
        return;
    }

    ID3D11DeviceContext* Context = RHI->GetDeviceContext();
    if (Context == nullptr)
    {
        return;
    }

    Context->IASetInputLayout(InputLayout.Get());
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    Context->VSSetShader(VertexShader.Get(), nullptr, 0);
    Context->PSSetShader(PixelShader.Get(), nullptr, 0);

    Context->VSSetConstantBuffers(0, 1, ConstantBuffer.GetAddressOf());
    Context->PSSetConstantBuffers(0, 1, ConstantBuffer.GetAddressOf());

    Context->OMSetDepthStencilState(DepthStencilState.Get(), 0);

    if (ViewMode == EViewModeIndex::Wireframe)
    {
        BindWireframeRasterizer();
    }
    else
    {
        BindSolidRasterizer();
    }
}

void FD3D11MeshBatchRenderer::BindSolidRasterizer()
{
    if (RHI == nullptr)
    {
        return;
    }

    ID3D11DeviceContext* Context = RHI->GetDeviceContext();
    if (Context == nullptr)
    {
        return;
    }

    Context->RSSetState(SolidRasterizerState.Get());
}

void FD3D11MeshBatchRenderer::BindWireframeRasterizer()
{
    if (RHI == nullptr)
    {
        return;
    }

    ID3D11DeviceContext* Context = RHI->GetDeviceContext();
    if (Context == nullptr)
    {
        return;
    }

    Context->RSSetState(WireframeRasterizerState.Get());
}

void FD3D11MeshBatchRenderer::DrawMeshBatch(EBasicMeshType InType, const FSceneView* InSceneView)
{
    if (RHI == nullptr || InSceneView == nullptr)
    {
        return;
    }

    ID3D11DeviceContext*             Context = RHI->GetDeviceContext();
    const FBasicMeshResource*        Resource = GetMeshResource(InType);
    const TArray<FMeshInstanceData>& Instances = MeshInstances[ToMeshIndex(InType)];

    if (Context == nullptr || Resource == nullptr || Resource->VertexBuffer == nullptr ||
        Resource->IndexBuffer == nullptr || Resource->IndexCount == 0 || Instances.empty() ||
        InstanceBuffer == nullptr)
    {
        return;
    }

    UINT Strides[2] = {sizeof(FMeshVertex), sizeof(FMeshInstanceData)};
    UINT Offsets[2] = {0, 0};

    ID3D11Buffer* VertexBuffers[2] = {
        Resource->VertexBuffer.Get(),
        InstanceBuffer.Get(),
    };

    Context->IASetVertexBuffers(0, 2, VertexBuffers, Strides, Offsets);
    Context->IASetIndexBuffer(Resource->IndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

    uint32 Remaining = static_cast<uint32>(Instances.size());
    uint32 StartIndex = 0;

    while (Remaining > 0)
    {
        const uint32 BatchCount = (Remaining > MaxInstanceCount) ? MaxInstanceCount : Remaining;

        if (!UploadInstanceRange(Context, InstanceBuffer.Get(), Instances.data() + StartIndex,
                                 BatchCount))
        {
            return;
        }

        Context->DrawIndexedInstanced(Resource->IndexCount, BatchCount, 0, 0, 0);

        StartIndex += BatchCount;
        Remaining -= BatchCount;
    }
}

FBasicMeshResource* FD3D11MeshBatchRenderer::GetMeshResource(EBasicMeshType InType)
{
    const uint32 Index = ToMeshIndex(InType);
    if (Index >= static_cast<uint32>(EBasicMeshType::Count))
    {
        return nullptr;
    }

    return &BasicMeshes[Index];
}

const FBasicMeshResource* FD3D11MeshBatchRenderer::GetMeshResource(EBasicMeshType InType) const
{
    const uint32 Index = ToMeshIndex(InType);
    if (Index >= static_cast<uint32>(EBasicMeshType::Count))
    {
        return nullptr;
    }

    return &BasicMeshes[Index];
}