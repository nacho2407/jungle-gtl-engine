#include "Renderer/D3D11/D3D11OutlineRenderer.h"

#include "Renderer/D3D11/D3D11RHI.h"
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

namespace
{
    constexpr FColor VisibleOutlineColor(1.0f, 0.72f, 0.20f, 1.0f);
    constexpr FColor OccludedOutlineColor(0.82f, 0.42f, 0.08f, 1.0f);
} // namespace

bool FD3D11OutlineRenderer::Initialize(FD3D11RHI* InRHI)
{
    if (InRHI == nullptr)
    {
        return false;
    }

    RHI = InRHI;
    CurrentSceneView = nullptr;

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

    RenderItems.clear();
    return true;
}

void FD3D11OutlineRenderer::Shutdown()
{
    RenderItems.clear();
    ReleaseBasicMeshes();

    StencilMaskBlendState.Reset();
    StencilMaskRasterizerState.Reset();
    OutlineRasterizerState.Reset();
    StencilMarkDepthStencilState.Reset();
    OccludedDepthStencilState.Reset();
    VisibleDepthStencilState.Reset();

    ConstantBuffer.Reset();
    InputLayout.Reset();
    PixelShader.Reset();
    VertexShader.Reset();

    CurrentSceneView = nullptr;
    RHI = nullptr;
}

void FD3D11OutlineRenderer::BeginFrame(const FSceneView* InSceneView)
{
    CurrentSceneView = InSceneView;
    RenderItems.clear();
}

void FD3D11OutlineRenderer::AddPrimitive(const FPrimitiveRenderItem& InItem)
{
    const int32 MeshTypeIndex = static_cast<int32>(InItem.MeshType);
    if (MeshTypeIndex < 0 || MeshTypeIndex >= static_cast<int32>(EBasicMeshType::Count))
    {
        return;
    }

    if (!InItem.State.IsVisible() || !InItem.State.IsSelected())
    {
        return;
    }

    FOutlineRenderItem OutlineItem = {};
    OutlineItem.World = InItem.World;
    OutlineItem.MeshType = InItem.MeshType;
    RenderItems.push_back(OutlineItem);
}

void FD3D11OutlineRenderer::AddPrimitives(const TArray<FPrimitiveRenderItem>& InItems)
{
    for (const FPrimitiveRenderItem& Item : InItems)
    {
        AddPrimitive(Item);
    }
}

void FD3D11OutlineRenderer::EndFrame()
{
    if (RHI == nullptr || CurrentSceneView == nullptr)
    {
        RenderItems.clear();
        CurrentSceneView = nullptr;
        return;
    }

    if (RenderItems.empty())
    {
        CurrentSceneView = nullptr;
        return;
    }

    RHI->SetDefaultRenderTargets();

    const float BlendFactor[4] = {0.f, 0.f, 0.f, 0.f};
    RHI->SetBlendState(StencilMaskBlendState.Get(), BlendFactor);
    BindPipeline(StencilMaskRasterizerState.Get(), StencilMarkDepthStencilState.Get());
    DrawItems(FColor::White(), 1.0f);
    RHI->ClearBlendState();

    BindPipeline(OutlineRasterizerState.Get(), VisibleDepthStencilState.Get());
    DrawItems(GetVisibleOutlineColor(), DefaultOutlineScale);

    RenderItems.clear();
    CurrentSceneView = nullptr;
}

FColor FD3D11OutlineRenderer::GetVisibleOutlineColor() { return VisibleOutlineColor; }

FColor FD3D11OutlineRenderer::GetOccludedOutlineColor() { return OccludedOutlineColor; }

bool FD3D11OutlineRenderer::CreateShaders()
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

bool FD3D11OutlineRenderer::CreateConstantBuffer()
{
    return (RHI != nullptr) &&
           RHI->CreateConstantBuffer(sizeof(FMeshUnlitConstants), ConstantBuffer.GetAddressOf());
}

bool FD3D11OutlineRenderer::CreateStates()
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

        if (FAILED(Device->CreateRasterizerState(&Desc,
                                                 StencilMaskRasterizerState.GetAddressOf())))
        {
            return false;
        }

        Desc.FillMode = D3D11_FILL_SOLID;
        Desc.CullMode = D3D11_CULL_FRONT;
        Desc.FrontCounterClockwise = FALSE;
        Desc.DepthClipEnable = TRUE;

        if (FAILED(Device->CreateRasterizerState(&Desc, OutlineRasterizerState.GetAddressOf())))
        {
            return false;
        }
    }

    {
        D3D11_BLEND_DESC BlendDesc = {};
        BlendDesc.AlphaToCoverageEnable = FALSE;
        BlendDesc.IndependentBlendEnable = FALSE;
        BlendDesc.RenderTarget[0].BlendEnable = FALSE;
        BlendDesc.RenderTarget[0].RenderTargetWriteMask = 0;

        if (FAILED(Device->CreateBlendState(&BlendDesc, StencilMaskBlendState.GetAddressOf())))
        {
            return false;
        }
    }

    {
        D3D11_DEPTH_STENCIL_DESC StencilDesc = {};
        StencilDesc.DepthEnable = TRUE;
        StencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        StencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        StencilDesc.StencilEnable = TRUE;
        StencilDesc.StencilReadMask = 0xFF;
        StencilDesc.StencilWriteMask = 0xFF;
        StencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        StencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
        StencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        StencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        StencilDesc.BackFace = StencilDesc.FrontFace;

        if (FAILED(Device->CreateDepthStencilState(&StencilDesc,
                                                   StencilMarkDepthStencilState.GetAddressOf())))
        {
            return false;
        }
    }

    {
        D3D11_DEPTH_STENCIL_DESC VisibleDesc = {};
        VisibleDesc.DepthEnable = TRUE;
        VisibleDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        VisibleDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        VisibleDesc.StencilEnable = TRUE;
        VisibleDesc.StencilReadMask = 0xFF;
        VisibleDesc.StencilWriteMask = 0x00;
        VisibleDesc.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
        VisibleDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        VisibleDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        VisibleDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        VisibleDesc.BackFace = VisibleDesc.FrontFace;

        if (FAILED(Device->CreateDepthStencilState(&VisibleDesc,
                                                   VisibleDepthStencilState.GetAddressOf())))
        {
            return false;
        }

        D3D11_DEPTH_STENCIL_DESC OccludedDesc = VisibleDesc;
        OccludedDesc.DepthFunc = D3D11_COMPARISON_GREATER;
        if (FAILED(Device->CreateDepthStencilState(&OccludedDesc,
                                                   OccludedDepthStencilState.GetAddressOf())))
        {
            return false;
        }
    }

    return true;
}

bool FD3D11OutlineRenderer::CreateBasicMeshes()
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

void FD3D11OutlineRenderer::ReleaseBasicMeshes()
{
    for (int32 Index = 0; Index < static_cast<int32>(EBasicMeshType::Count); ++Index)
    {
        MeshResources[Index].VertexBuffer.Reset();
        MeshResources[Index].IndexBuffer.Reset();
        MeshResources[Index].IndexCount = 0;
        MeshResources[Index].Topology = EMeshPrimitiveTopology::TriangleList;
    }
}

bool FD3D11OutlineRenderer::CreateBasicMeshResource(const FVertexSimple* InVertices,
                                                    uint32 InVertexCount, const uint16* InIndices,
                                                    uint32                 InIndexCount,
                                                    EMeshPrimitiveTopology InTopology,
                                                    FOutlineMeshResource&  OutResource)
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

bool FD3D11OutlineRenderer::CreateBasicCubeMesh(FOutlineMeshResource& OutResource)
{
    return CreateBasicMeshResource(cube_vertices, cube_vertex_count, cube_indices, cube_index_count,
                                   cube_topology, OutResource);
}

bool FD3D11OutlineRenderer::CreateBasicQuadMesh(FOutlineMeshResource& OutResource)
{
    return CreateBasicMeshResource(quad_vertices, quad_vertex_count, quad_indices, quad_index_count,
                                   quad_topology, OutResource);
}

bool FD3D11OutlineRenderer::CreateBasicTriangleMesh(FOutlineMeshResource& OutResource)
{
    return CreateBasicMeshResource(triangle_vertices, triangle_vertex_count, triangle_indices,
                                   triangle_index_count, triangle_topology, OutResource);
}

bool FD3D11OutlineRenderer::CreateBasicSphereMesh(FOutlineMeshResource& OutResource)
{
    return CreateBasicMeshResource(sphere_vertices, sphere_vertex_count, sphere_indices,
                                   sphere_index_count, sphere_topology, OutResource);
}

bool FD3D11OutlineRenderer::CreateBasicConeMesh(FOutlineMeshResource& OutResource)
{
    return CreateBasicMeshResource(cone_vertices, cone_vertex_count, cone_indices, cone_index_count,
                                   cone_topology, OutResource);
}

bool FD3D11OutlineRenderer::CreateBasicCylinderMesh(FOutlineMeshResource& OutResource)
{
    return CreateBasicMeshResource(cylinder_vertices, cylinder_vertex_count, cylinder_indices,
                                   cylinder_index_count, cylinder_topology, OutResource);
}

bool FD3D11OutlineRenderer::CreateBasicRingMesh(FOutlineMeshResource& OutResource)
{
    return CreateBasicMeshResource(ring_vertices, ring_vertex_count, ring_indices, ring_index_count,
                                   ring_topology, OutResource);
}

FOutlineMeshResource* FD3D11OutlineRenderer::GetBasicMeshResource(EBasicMeshType InType)
{
    const int32 Index = static_cast<int32>(InType);
    if (Index < 0 || Index >= static_cast<int32>(EBasicMeshType::Count))
    {
        return nullptr;
    }

    return &MeshResources[Index];
}

void FD3D11OutlineRenderer::BindPipeline(ID3D11RasterizerState* InRasterizerState,
                                         ID3D11DepthStencilState* InDepthStencilState)
{
    if (RHI == nullptr)
    {
        return;
    }

    RHI->SetInputLayout(InputLayout.Get());
    RHI->SetVertexShader(VertexShader.Get());
    RHI->SetVSConstantBuffer(0, ConstantBuffer.Get());
    RHI->SetPixelShader(PixelShader.Get());

    RHI->SetRasterizerState(InRasterizerState);
    RHI->SetDepthStencilState(InDepthStencilState, 1);
}

void FD3D11OutlineRenderer::BindPrimitiveTopology(EMeshPrimitiveTopology InTopology)
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

void FD3D11OutlineRenderer::DrawItems(const FColor& InColor, float InScale)
{
    if (RHI == nullptr || CurrentSceneView == nullptr)
    {
        return;
    }

    for (const FOutlineRenderItem& Item : RenderItems)
    {
        FOutlineMeshResource* MeshResource = GetBasicMeshResource(Item.MeshType);
        if (MeshResource == nullptr || MeshResource->VertexBuffer == nullptr ||
            MeshResource->IndexBuffer == nullptr || MeshResource->IndexCount == 0)
        {
            continue;
        }

        BindPrimitiveTopology(MeshResource->Topology);

        const UINT    Stride = sizeof(FVertexSimple);
        const UINT    Offset = 0;
        ID3D11Buffer* VertexBuffer = MeshResource->VertexBuffer.Get();

        RHI->SetVertexBuffer(0, VertexBuffer, Stride, Offset);
        RHI->SetIndexBuffer(MeshResource->IndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

        FMeshUnlitConstants Constants = {};
        Constants.MVP =
            Item.World.ApplyScale(InScale) * CurrentSceneView->GetViewProjectionMatrix();
        Constants.BaseColor = InColor;

        if (!RHI->UpdateConstantBuffer(ConstantBuffer.Get(), &Constants, sizeof(Constants)))
        {
            continue;
        }

        RHI->DrawIndexed(MeshResource->IndexCount, 0, 0);
    }
}
