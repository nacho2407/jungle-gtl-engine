#include "Renderer/D3D11/D3D11MeshBatchRenderer.h"

#include "Renderer/D3D11/D3D11DynamicRHI.h"
#include "Renderer/SceneView.h"
#include "Renderer/Types/ShaderConstants.h"
#include "Renderer/Types/VertexTypes.h"

#include "Resources/Mesh/Cone.h"
#include "Resources/Mesh/Cube.h"
#include "Resources/Mesh/Cylinder.h"
#include "Resources/Mesh/Quad.h"
#include "Resources/Mesh/Ring.h"
#include "Resources/Mesh/Sphere.h"
#include "Resources/Mesh/Triangle.h"

bool FD3D11MeshBatchRenderer::Initialize(FD3D11DynamicRHI* InRHI)
{
    if (InRHI == nullptr)
    {
        return false;
    }

    RHI = InRHI;
    CurrentSceneView = nullptr;
    bUseInstancing = true;

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

    CurrentSceneView = nullptr;
    bUseInstancing = true;

    DepthStencilState.Reset();
    WireframeRasterizerState.Reset();
    SolidRasterizerState.Reset();

    InstanceBuffer.Reset();
    InstancedConstantBuffer.Reset();
    SingleConstantBuffer.Reset();

    InstancedInputLayout.Reset();
    InstancedVertexShader.Reset();
    InstancedPixelShader.Reset();

    SingleInputLayout.Reset();
    SingleVertexShader.Reset();
    SinglePixelShader.Reset();

    RHI = nullptr;
}

void FD3D11MeshBatchRenderer::BeginFrame(const FSceneView* InSceneView, EViewModeIndex InViewMode,
                                         bool bInUseInstancing)
{
    CurrentSceneView = InSceneView;
    ViewMode = InViewMode;
    bUseInstancing = bInUseInstancing;

    ResetBatches();
}

void FD3D11MeshBatchRenderer::AddPrimitive(const FPrimitiveRenderItem& InItem)
{
    const int32 MeshTypeIndex = static_cast<int32>(InItem.MeshType);
    if (MeshTypeIndex < 0 || MeshTypeIndex >= static_cast<int32>(EBasicMeshType::Count))
    {
        return;
    }

    if (!InItem.bVisible)
    {
        return;
    }

    FMeshDrawData DrawData = {};
    DrawData.World = InItem.World;
    DrawData.Color = InItem.Color;

    MeshDraws[MeshTypeIndex].push_back(DrawData);
}

void FD3D11MeshBatchRenderer::AddPrimitives(const TArray<FPrimitiveRenderItem>& InItems)
{
    for (const FPrimitiveRenderItem& Item : InItems)
    {
        AddPrimitive(Item);
    }
}

void FD3D11MeshBatchRenderer::EndFrame()
{
    if (RHI == nullptr)
    {
        return;
    }

    Flush();
    ResetBatches();
    CurrentSceneView = nullptr;
}

void FD3D11MeshBatchRenderer::Flush()
{
    if (RHI == nullptr || CurrentSceneView == nullptr)
    {
        return;
    }

    FlushInternal(bUseInstancing ? EMeshDrawPath::Instanced : EMeshDrawPath::Single,
                  CurrentSceneView);
}

bool FD3D11MeshBatchRenderer::CreateShaders()
{
    if (RHI == nullptr)
    {
        return false;
    }

    static const D3D11_INPUT_ELEMENT_DESC InstancedLayoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},

        {"WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1},

        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    };

    static const D3D11_INPUT_ELEMENT_DESC SingleLayoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    if (!RHI->CreateVertexShaderAndInputLayout(InstancedShaderPath, "VSMain", InstancedLayoutDesc,
                                               static_cast<uint32>(std::size(InstancedLayoutDesc)),
                                               InstancedVertexShader.GetAddressOf(),
                                               InstancedInputLayout.GetAddressOf()))
    {
        return false;
    }

    if (!RHI->CreatePixelShader(InstancedShaderPath, "PSMain", InstancedPixelShader.GetAddressOf()))
    {
        InstancedInputLayout.Reset();
        InstancedVertexShader.Reset();
        return false;
    }

    if (!RHI->CreateVertexShaderAndInputLayout(SingleShaderPath, "VSMain", SingleLayoutDesc,
                                               static_cast<uint32>(std::size(SingleLayoutDesc)),
                                               SingleVertexShader.GetAddressOf(),
                                               SingleInputLayout.GetAddressOf()))
    {
        InstancedPixelShader.Reset();
        InstancedInputLayout.Reset();
        InstancedVertexShader.Reset();
        return false;
    }

    if (!RHI->CreatePixelShader(SingleShaderPath, "PSMain", SinglePixelShader.GetAddressOf()))
    {
        SingleInputLayout.Reset();
        SingleVertexShader.Reset();

        InstancedPixelShader.Reset();
        InstancedInputLayout.Reset();
        InstancedVertexShader.Reset();
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

    const bool bSingleOk =
        RHI->CreateConstantBuffer(sizeof(FMeshUnlitConstants), SingleConstantBuffer.GetAddressOf());

    const bool bInstancedOk = RHI->CreateConstantBuffer(sizeof(FMeshUnlitInstancedConstants),
                                                        InstancedConstantBuffer.GetAddressOf());

    return bSingleOk && bInstancedOk;
}
bool FD3D11MeshBatchRenderer::CreateStates()
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

        if (FAILED(Device->CreateRasterizerState(&Desc, SolidRasterizerState.GetAddressOf())))
        {
            return false;
        }

        Desc.FillMode = D3D11_FILL_WIREFRAME;
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

        if (FAILED(Device->CreateDepthStencilState(&Desc, DepthStencilState.GetAddressOf())))
        {
            return false;
        }
    }

    return true;
}

bool FD3D11MeshBatchRenderer::CreateDynamicInstanceBuffer(uint32 InMaxInstanceCount)
{
    if (RHI == nullptr)
    {
        return false;
    }

    return RHI->CreateVertexBuffer(nullptr, sizeof(FMeshDrawData) * InMaxInstanceCount,
                                   sizeof(FMeshDrawData), true, InstanceBuffer.GetAddressOf());
}

bool FD3D11MeshBatchRenderer::CreateBasicMeshes()
{
    bool bOk = true;
    bOk &= CreateBasicCubeMesh(BasicMeshResources[static_cast<int32>(EBasicMeshType::Cube)]);
    bOk &= CreateBasicQuadMesh(BasicMeshResources[static_cast<int32>(EBasicMeshType::Quad)]);
    bOk &=
        CreateBasicTriangleMesh(BasicMeshResources[static_cast<int32>(EBasicMeshType::Triangle)]);
    bOk &= CreateBasicSphereMesh(BasicMeshResources[static_cast<int32>(EBasicMeshType::Sphere)]);
    bOk &= CreateBasicConeMesh(BasicMeshResources[static_cast<int32>(EBasicMeshType::Cone)]);
    bOk &=
        CreateBasicCylinderMesh(BasicMeshResources[static_cast<int32>(EBasicMeshType::Cylinder)]);
    bOk &= CreateBasicRingMesh(BasicMeshResources[static_cast<int32>(EBasicMeshType::Ring)]);
    return bOk;
}

bool FD3D11MeshBatchRenderer::CreateBasicCubeMesh(FBasicMeshResource& OutResource)
{
    return CreateBasicMeshResource(cube_vertices, cube_vertex_count, cube_indices, cube_index_count,
                                   cube_topology, OutResource);
}

bool FD3D11MeshBatchRenderer::CreateBasicQuadMesh(FBasicMeshResource& OutResource)
{
    return CreateBasicMeshResource(quad_vertices, quad_vertex_count, quad_indices, quad_index_count,
                                   quad_topology, OutResource);
}

bool FD3D11MeshBatchRenderer::CreateBasicTriangleMesh(FBasicMeshResource& OutResource)
{
    return CreateBasicMeshResource(triangle_vertices, triangle_vertex_count, triangle_indices,
                                   triangle_index_count, triangle_topology, OutResource);
}

bool FD3D11MeshBatchRenderer::CreateBasicSphereMesh(FBasicMeshResource& OutResource)
{
    return CreateBasicMeshResource(sphere_vertices, sphere_vertex_count, sphere_indices,
                                   sphere_index_count, sphere_topology, OutResource);
}

bool FD3D11MeshBatchRenderer::CreateBasicConeMesh(FBasicMeshResource& OutResource)
{
    return CreateBasicMeshResource(cone_vertices, cone_vertex_count, cone_indices, cone_index_count,
                                   cone_topology, OutResource);
}

bool FD3D11MeshBatchRenderer::CreateBasicCylinderMesh(FBasicMeshResource& OutResource)
{
    return CreateBasicMeshResource(cylinder_vertices, cylinder_vertex_count, cylinder_indices,
                                   cylinder_index_count, cylinder_topology, OutResource);
}

bool FD3D11MeshBatchRenderer::CreateBasicRingMesh(FBasicMeshResource& OutResource)
{
    return CreateBasicMeshResource(ring_vertices, ring_vertex_count, ring_indices, ring_index_count,
                                   ring_topology, OutResource);
}

void FD3D11MeshBatchRenderer::ReleaseBasicMeshes()
{
    for (int32 Index = 0; Index < static_cast<int32>(EBasicMeshType::Count); ++Index)
    {
        BasicMeshResources[Index].VertexBuffer.Reset();
        BasicMeshResources[Index].IndexBuffer.Reset();
        BasicMeshResources[Index].IndexCount = 0;
        BasicMeshResources[Index].Topology = EMeshPrimitiveTopology::TriangleList;
    }
}

bool FD3D11MeshBatchRenderer::CreateBasicMeshResource(const FVertexSimple* InVertices,
                                                      uint32 InVertexCount, const uint16* InIndices,
                                                      uint32                 InIndexCount,
                                                      EMeshPrimitiveTopology InTopology,
                                                      FBasicMeshResource&    OutResource)
{
    if (RHI == nullptr || RHI->GetDevice() == nullptr || InVertices == nullptr ||
        InIndices == nullptr || InVertexCount == 0 || InIndexCount == 0)
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

void FD3D11MeshBatchRenderer::ResetBatches()
{
    for (int32 Index = 0; Index < static_cast<int32>(EBasicMeshType::Count); ++Index)
    {
        MeshDraws[Index].clear();
    }
}

void FD3D11MeshBatchRenderer::UpdatePerFrameConstants(const FSceneView* InSceneView,
                                                      EMeshDrawPath     DrawPath)
{
    if (RHI == nullptr || InSceneView == nullptr)
    {
        return;
    }

    if (DrawPath == EMeshDrawPath::Instanced)
    {
        FMeshUnlitInstancedConstants Constants = {};
        Constants.VP = InSceneView->GetViewProjectionMatrix();
        RHI->UpdateConstantBuffer(InstancedConstantBuffer.Get(), &Constants, sizeof(Constants));
    }
}

void FD3D11MeshBatchRenderer::BindPipeline(EMeshDrawPath DrawPath)
{
    if (RHI == nullptr)
    {
        return;
    }

    if (DrawPath == EMeshDrawPath::Instanced)
    {
        RHI->SetInputLayout(InstancedInputLayout.Get());
        RHI->SetVertexShader(InstancedVertexShader.Get());
        RHI->SetVSConstantBuffer(0, InstancedConstantBuffer.Get());
        RHI->SetPixelShader(InstancedPixelShader.Get());
    }
    else
    {
        RHI->SetInputLayout(SingleInputLayout.Get());
        RHI->SetVertexShader(SingleVertexShader.Get());
        RHI->SetVSConstantBuffer(0, SingleConstantBuffer.Get());
        RHI->SetPixelShader(SinglePixelShader.Get());
    }
}

void FD3D11MeshBatchRenderer::BindPrimitiveTopology(EMeshPrimitiveTopology InTopology)
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

void FD3D11MeshBatchRenderer::BindSolidRasterizer()
{
    if (RHI == nullptr)
    {
        return;
    }

    RHI->SetRasterizerState(SolidRasterizerState.Get());
}

void FD3D11MeshBatchRenderer::BindWireframeRasterizer()
{
    if (RHI == nullptr)
    {
        return;
    }

    RHI->SetRasterizerState(WireframeRasterizerState.Get());
}

void FD3D11MeshBatchRenderer::FlushInternal(EMeshDrawPath DrawPath, const FSceneView* InSceneView)
{
    if (RHI == nullptr || InSceneView == nullptr)
    {
        return;
    }

    BindPipeline(DrawPath);

    if (ViewMode == EViewModeIndex::Wireframe)
    {
        BindWireframeRasterizer();
    }
    else
    {
        BindSolidRasterizer();
    }

    UpdatePerFrameConstants(InSceneView, DrawPath);

    for (int32 Index = 0; Index < static_cast<int32>(EBasicMeshType::Count); ++Index)
    {
        DrawMeshBatch(static_cast<EBasicMeshType>(Index), DrawPath, InSceneView);
    }
}

void FD3D11MeshBatchRenderer::DrawMeshBatch(EBasicMeshType InType, EMeshDrawPath DrawPath,
                                            const FSceneView* InSceneView)
{
    if (RHI == nullptr || InSceneView == nullptr)
    {
        return;
    }

    const int32 TypeIndex = static_cast<int32>(InType);
    if (TypeIndex < 0 || TypeIndex >= static_cast<int32>(EBasicMeshType::Count))
    {
        return;
    }

    FBasicMeshResource* MeshResource = GetBasicMeshResource(InType);
    if (MeshResource == nullptr || MeshResource->VertexBuffer == nullptr ||
        MeshResource->IndexBuffer == nullptr || MeshResource->IndexCount == 0)
    {
        return;
    }

    TArray<FMeshDrawData>& Draws = MeshDraws[TypeIndex];
    if (Draws.empty())
    {
        return;
    }

    BindPrimitiveTopology(MeshResource->Topology);

    if (DrawPath == EMeshDrawPath::Instanced)
    {
        const uint32 InstanceCount = static_cast<uint32>(Draws.size());
        if (InstanceCount == 0 || InstanceCount > MaxInstanceCount)
        {
            return;
        }

        if (!RHI->UpdateDynamicBuffer(InstanceBuffer.Get(), Draws.data(),
                                      static_cast<uint32>(sizeof(FMeshDrawData) * Draws.size())))
        {
            return;
        }

        ID3D11Buffer* VertexBuffers[2] = {
            MeshResource->VertexBuffer.Get(),
            InstanceBuffer.Get(),
        };

        const UINT Strides[2] = {
            sizeof(FVertexSimple),
            sizeof(FMeshDrawData),
        };

        const UINT Offsets[2] = {0, 0};

        RHI->SetVertexBuffers(0, 2, VertexBuffers, Strides, Offsets);
        RHI->SetIndexBuffer(MeshResource->IndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
        RHI->DrawIndexedInstanced(MeshResource->IndexCount, InstanceCount, 0, 0, 0);
    }
    else
    {
        const UINT    Stride = sizeof(FVertexSimple);
        const UINT    Offset = 0;
        ID3D11Buffer* VertexBuffer = MeshResource->VertexBuffer.Get();

        RHI->SetVertexBuffer(0, VertexBuffer, Stride, Offset);
        RHI->SetIndexBuffer(MeshResource->IndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

        for (const FMeshDrawData& Draw : Draws)
        {
            FMeshUnlitConstants Constants = {};
            Constants.MVP = Draw.World * InSceneView->GetViewProjectionMatrix();
            Constants.BaseColor = Draw.Color;

            if (!RHI->UpdateConstantBuffer(SingleConstantBuffer.Get(), &Constants,
                                           sizeof(Constants)))
            {
                continue;
            }

            RHI->DrawIndexed(MeshResource->IndexCount, 0, 0);
        }
    }
}

FBasicMeshResource* FD3D11MeshBatchRenderer::GetBasicMeshResource(EBasicMeshType InType)
{
    const int32 Index = static_cast<int32>(InType);
    if (Index < 0 || Index >= static_cast<int32>(EBasicMeshType::Count))
    {
        return nullptr;
    }

    return &BasicMeshResources[Index];
}

const FBasicMeshResource* FD3D11MeshBatchRenderer::GetBasicMeshResource(EBasicMeshType InType) const
{
    const int32 Index = static_cast<int32>(InType);
    if (Index < 0 || Index >= static_cast<int32>(EBasicMeshType::Count))
    {
        return nullptr;
    }

    return &BasicMeshResources[Index];
}