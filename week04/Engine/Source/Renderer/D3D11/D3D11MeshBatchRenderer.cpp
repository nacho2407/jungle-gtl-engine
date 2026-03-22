#include "D3D11MeshBatchRenderer.h"

#include "Renderer/SceneView.h"
#include "Renderer/Types/ShaderConstants.h"
#include "Renderer/Types/VertexTypes.h"

#include "Renderer/D3D11/D3D11DynamicRHI.h"

#include "Resources/Mesh/VertexSimple.h"
#include "Resources/Mesh/Cube.h"
#include "Resources/Mesh/Plane.h"
#include "Resources/Mesh/Triangle.h"
#include "Resources/Mesh/Sphere.h"
#include "Resources/Mesh/Cone.h"
#include "Resources/Mesh/Cylinder.h"
#include "Resources/Mesh/Ring.h"

#include <d3dcompiler.h>

namespace
{
    constexpr uint32 ToMeshIndex(EBasicMeshType InType) { return static_cast<uint32>(InType); }
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

void FD3D11MeshBatchRenderer::Render(const FSceneRenderData& InRenderData)
{
    ResetBatches();
    GatherRenderItems(InRenderData);

    if (InRenderData.SceneView == nullptr)
    {
        return;
    }

    Flush(InRenderData.bUseInstancing ? EMeshDrawPath::Instanced : EMeshDrawPath::Single,
          InRenderData.SceneView);
}

bool FD3D11MeshBatchRenderer::CreateShaders()
{
    if (RHI == nullptr || RHI->GetDevice() == nullptr)
    {
        return false;
    }

    ID3D11Device* Device = RHI->GetDevice();

    TComPtr<ID3DBlob> InstancedVSBlob;
    TComPtr<ID3DBlob> InstancedPSBlob;
    TComPtr<ID3DBlob> SingleVSBlob;
    TComPtr<ID3DBlob> SinglePSBlob;
    TComPtr<ID3DBlob> ErrorBlob;

    UINT CompileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    CompileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT Hr = D3DCompileFromFile(InstancedShaderPath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                                    "VSMain", "vs_5_0", CompileFlags, 0,
                                    InstancedVSBlob.GetAddressOf(), ErrorBlob.GetAddressOf());
    if (FAILED(Hr))
    {
        return false;
    }

    ErrorBlob.Reset();

    Hr = D3DCompileFromFile(InstancedShaderPath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                            "PSMain", "ps_5_0", CompileFlags, 0, InstancedPSBlob.GetAddressOf(),
                            ErrorBlob.GetAddressOf());
    if (FAILED(Hr))
    {
        return false;
    }

    ErrorBlob.Reset();

    Hr = D3DCompileFromFile(SingleShaderPath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain",
                            "vs_5_0", CompileFlags, 0, SingleVSBlob.GetAddressOf(),
                            ErrorBlob.GetAddressOf());
    if (FAILED(Hr))
    {
        return false;
    }

    ErrorBlob.Reset();

    Hr = D3DCompileFromFile(SingleShaderPath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain",
                            "ps_5_0", CompileFlags, 0, SinglePSBlob.GetAddressOf(),
                            ErrorBlob.GetAddressOf());
    if (FAILED(Hr))
    {
        return false;
    }

    Hr = Device->CreateVertexShader(InstancedVSBlob->GetBufferPointer(),
                                    InstancedVSBlob->GetBufferSize(), nullptr,
                                    InstancedVertexShader.GetAddressOf());
    if (FAILED(Hr))
    {
        return false;
    }

    Hr = Device->CreatePixelShader(InstancedPSBlob->GetBufferPointer(),
                                   InstancedPSBlob->GetBufferSize(), nullptr,
                                   InstancedPixelShader.GetAddressOf());
    if (FAILED(Hr))
    {
        return false;
    }

    Hr = Device->CreateVertexShader(SingleVSBlob->GetBufferPointer(), SingleVSBlob->GetBufferSize(),
                                    nullptr, SingleVertexShader.GetAddressOf());
    if (FAILED(Hr))
    {
        return false;
    }

    Hr = Device->CreatePixelShader(SinglePSBlob->GetBufferPointer(), SinglePSBlob->GetBufferSize(),
                                   nullptr, SinglePixelShader.GetAddressOf());
    if (FAILED(Hr))
    {
        return false;
    }

    {
        const D3D11_INPUT_ELEMENT_DESC InstancedLayoutDesc[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},

            {"WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
            {"WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1},
            {"WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1},
            {"WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1},

            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        };

        Hr = Device->CreateInputLayout(InstancedLayoutDesc, ARRAYSIZE(InstancedLayoutDesc),
                                       InstancedVSBlob->GetBufferPointer(),
                                       InstancedVSBlob->GetBufferSize(),
                                       InstancedInputLayout.GetAddressOf());
        if (FAILED(Hr))
        {
            return false;
        }
    }

    {
        const D3D11_INPUT_ELEMENT_DESC SingleLayoutDesc[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };

        Hr = Device->CreateInputLayout(
            SingleLayoutDesc, ARRAYSIZE(SingleLayoutDesc), SingleVSBlob->GetBufferPointer(),
            SingleVSBlob->GetBufferSize(), SingleInputLayout.GetAddressOf());
        if (FAILED(Hr))
        {
            return false;
        }
    }

    return true;
}

bool FD3D11MeshBatchRenderer::CreateConstantBuffers()
{
    if (RHI == nullptr)
    {
        return false;
    }

    static_assert((sizeof(FMeshUnlitConstants) % 16) == 0);
    static_assert((sizeof(FMeshUnlitInstancedConstants) % 16) == 0);

    if (!RHI->CreateConstantBuffer(static_cast<uint32>(sizeof(FMeshUnlitConstants)),
                                   SingleConstantBuffer.GetAddressOf()))
    {
        return false;
    }

    if (!RHI->CreateConstantBuffer(static_cast<uint32>(sizeof(FMeshUnlitInstancedConstants)),
                                   InstancedConstantBuffer.GetAddressOf()))
    {
        return false;
    }

    return true;
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
    if (RHI == nullptr || RHI->GetDevice() == nullptr)
    {
        return false;
    }

    D3D11_BUFFER_DESC Desc = {};
    Desc.ByteWidth = sizeof(FMeshDrawData) * InMaxInstanceCount;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    return SUCCEEDED(RHI->GetDevice()->CreateBuffer(&Desc, nullptr, InstanceBuffer.GetAddressOf()));
}

bool FD3D11MeshBatchRenderer::CreateBasicMeshes()
{
    ReleaseBasicMeshes();

    return CreateBasicCubeMesh(BasicMeshResources[ToMeshIndex(EBasicMeshType::Cube)]) &&
           CreateBasicPlaneMesh(BasicMeshResources[ToMeshIndex(EBasicMeshType::Plane)]) &&
           CreateBasicTriangleMesh(BasicMeshResources[ToMeshIndex(EBasicMeshType::Triangle)]) &&
           CreateBasicSphereMesh(BasicMeshResources[ToMeshIndex(EBasicMeshType::Sphere)]) &&
           CreateBasicConeMesh(BasicMeshResources[ToMeshIndex(EBasicMeshType::Cone)]) &&
           CreateBasicCylinderMesh(BasicMeshResources[ToMeshIndex(EBasicMeshType::Cylinder)]) &&
           CreateBasicRingMesh(BasicMeshResources[ToMeshIndex(EBasicMeshType::Ring)]);
}

void FD3D11MeshBatchRenderer::ReleaseBasicMeshes()
{
    for (int32 Index = 0; Index < static_cast<int32>(EBasicMeshType::Count); ++Index)
    {
        BasicMeshResources[Index].VertexBuffer.Reset();
        BasicMeshResources[Index].IndexBuffer.Reset();
        BasicMeshResources[Index].IndexCount = 0;
    }
}

bool FD3D11MeshBatchRenderer::CreateBasicCubeMesh(FBasicMeshResource& OutResource)
{
    return CreateBasicMeshResource(cube_vertices, static_cast<uint32>(std::size(cube_vertices)),
                                   cube_indices, static_cast<uint32>(std::size(cube_indices)),
                                   OutResource);
}

bool FD3D11MeshBatchRenderer::CreateBasicPlaneMesh(FBasicMeshResource& OutResource)
{
    return CreateBasicMeshResource(plane_vertices, static_cast<uint32>(std::size(plane_vertices)),
                                   plane_indices, static_cast<uint32>(std::size(plane_indices)),
                                   OutResource);
}

bool FD3D11MeshBatchRenderer::CreateBasicTriangleMesh(FBasicMeshResource& OutResource)
{
    return CreateBasicMeshResource(
        triangle_vertices, static_cast<uint32>(std::size(triangle_vertices)), triangle_indices,
        static_cast<uint32>(std::size(triangle_indices)), OutResource);
}

bool FD3D11MeshBatchRenderer::CreateBasicSphereMesh(FBasicMeshResource& OutResource)
{
    return CreateBasicMeshResource(sphere_vertices, static_cast<uint32>(std::size(sphere_vertices)),
                                   sphere_indices, static_cast<uint32>(std::size(sphere_indices)),
                                   OutResource);
}

bool FD3D11MeshBatchRenderer::CreateBasicConeMesh(FBasicMeshResource& OutResource)
{
    return CreateBasicMeshResource(cone_vertices, static_cast<uint32>(std::size(cone_vertices)),
                                   cone_indices, static_cast<uint32>(std::size(cone_indices)),
                                   OutResource);
}

bool FD3D11MeshBatchRenderer::CreateBasicCylinderMesh(FBasicMeshResource& OutResource)
{
    return CreateBasicMeshResource(
        cylinder_vertices, static_cast<uint32>(std::size(cylinder_vertices)), cylinder_indices,
        static_cast<uint32>(std::size(cylinder_indices)), OutResource);
}

bool FD3D11MeshBatchRenderer::CreateBasicRingMesh(FBasicMeshResource& OutResource)
{
    return CreateBasicMeshResource(ring_vertices, static_cast<uint32>(std::size(ring_vertices)),
                                   ring_indices, static_cast<uint32>(std::size(ring_indices)),
                                   OutResource);
}

bool FD3D11MeshBatchRenderer::CreateBasicMeshResource(const FVertexSimple* InVertices,
                                                      uint32 InVertexCount, const uint16* InIndices,
                                                      uint32              InIndexCount,
                                                      FBasicMeshResource& OutResource)
{
    if (RHI == nullptr || RHI->GetDevice() == nullptr || InVertices == nullptr ||
        InVertexCount == 0 || InIndices == nullptr || InIndexCount == 0)
    {
        return false;
    }

    ID3D11Device* Device = RHI->GetDevice();

    {
        D3D11_BUFFER_DESC Desc = {};
        Desc.ByteWidth = sizeof(FVertexSimple) * InVertexCount;
        Desc.Usage = D3D11_USAGE_IMMUTABLE;
        Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA InitData = {};
        InitData.pSysMem = InVertices;

        if (FAILED(Device->CreateBuffer(&Desc, &InitData, OutResource.VertexBuffer.GetAddressOf())))
        {
            return false;
        }
    }

    {
        D3D11_BUFFER_DESC Desc = {};
        Desc.ByteWidth = sizeof(uint16) * InIndexCount;
        Desc.Usage = D3D11_USAGE_IMMUTABLE;
        Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA InitData = {};
        InitData.pSysMem = InIndices;

        if (FAILED(Device->CreateBuffer(&Desc, &InitData, OutResource.IndexBuffer.GetAddressOf())))
        {
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
        MeshDraws[Index].clear();
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

        FMeshDrawData DrawData = {};
        DrawData.World = Item.World;
        DrawData.Color = Item.Color;

        if (Item.bSelected)
        {
            DrawData.Color.b *= 0.6f;
        }

        if (Item.bHovered)
        {
            DrawData.Color.r *= 1.15f;
            DrawData.Color.g *= 1.15f;
            DrawData.Color.b *= 1.15f;
        }

        MeshDraws[MeshIndex].push_back(DrawData);
    }
}

void FD3D11MeshBatchRenderer::Flush(EMeshDrawPath DrawPath, const FSceneView* InSceneView)
{
    if (RHI == nullptr || InSceneView == nullptr || RHI->GetDeviceContext() == nullptr)
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
    if (RHI == nullptr || RHI->GetDeviceContext() == nullptr)
    {
        return;
    }

    ID3D11DeviceContext* Context = RHI->GetDeviceContext();

    Context->OMSetDepthStencilState(DepthStencilState.Get(), 0);

    if (DrawPath == EMeshDrawPath::Instanced)
    {
        Context->IASetInputLayout(InstancedInputLayout.Get());
        Context->VSSetShader(InstancedVertexShader.Get(), nullptr, 0);
        Context->PSSetShader(InstancedPixelShader.Get(), nullptr, 0);

        ID3D11Buffer* VSConstantBuffers[] = {InstancedConstantBuffer.Get()};
        Context->VSSetConstantBuffers(0, 1, VSConstantBuffers);

        ID3D11Buffer* PSConstantBuffers[] = {InstancedConstantBuffer.Get()};
        Context->PSSetConstantBuffers(0, 1, PSConstantBuffers);
    }
    else
    {
        Context->IASetInputLayout(SingleInputLayout.Get());
        Context->VSSetShader(SingleVertexShader.Get(), nullptr, 0);
        Context->PSSetShader(SinglePixelShader.Get(), nullptr, 0);

        ID3D11Buffer* VSConstantBuffers[] = {SingleConstantBuffer.Get()};
        Context->VSSetConstantBuffers(0, 1, VSConstantBuffers);

        ID3D11Buffer* PSConstantBuffers[] = {SingleConstantBuffer.Get()};
        Context->PSSetConstantBuffers(0, 1, PSConstantBuffers);
    }

    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void FD3D11MeshBatchRenderer::BindSolidRasterizer()
{
    if (RHI == nullptr || RHI->GetDeviceContext() == nullptr)
    {
        return;
    }

    RHI->GetDeviceContext()->RSSetState(SolidRasterizerState.Get());
}

void FD3D11MeshBatchRenderer::BindWireframeRasterizer()
{
    if (RHI == nullptr || RHI->GetDeviceContext() == nullptr)
    {
        return;
    }

    RHI->GetDeviceContext()->RSSetState(WireframeRasterizerState.Get());
}

void FD3D11MeshBatchRenderer::DrawMeshBatch(EBasicMeshType InType, EMeshDrawPath DrawPath,
                                            const FSceneView* InSceneView)
{
    if (RHI == nullptr || RHI->GetDeviceContext() == nullptr || InSceneView == nullptr)
    {
        return;
    }

    const uint32 MeshIndex = ToMeshIndex(InType);
    if (MeshIndex >= static_cast<uint32>(EBasicMeshType::Count))
    {
        return;
    }

    const TArray<FMeshDrawData>& Draws = MeshDraws[MeshIndex];
    if (Draws.empty())
    {
        return;
    }

    FBasicMeshResource* MeshResource = GetBasicMeshResource(InType);
    if (MeshResource == nullptr || MeshResource->VertexBuffer == nullptr ||
        MeshResource->IndexBuffer == nullptr || MeshResource->IndexCount == 0)
    {
        return;
    }

    ID3D11DeviceContext* Context = RHI->GetDeviceContext();

    if (DrawPath == EMeshDrawPath::Instanced)
    {
        D3D11_MAPPED_SUBRESOURCE Mapped = {};
        if (FAILED(Context->Map(InstanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
        {
            return;
        }

        const uint32 InstanceCount = static_cast<uint32>(Draws.size());
        memcpy(Mapped.pData, Draws.data(), sizeof(FMeshDrawData) * InstanceCount);

        Context->Unmap(InstanceBuffer.Get(), 0);

        ID3D11Buffer* Buffers[] = {MeshResource->VertexBuffer.Get(), InstanceBuffer.Get()};
        UINT          Strides[] = {sizeof(FVertexSimple), sizeof(FMeshDrawData)};
        UINT          Offsets[] = {0, 0};

        Context->IASetVertexBuffers(0, 2, Buffers, Strides, Offsets);
        Context->IASetIndexBuffer(MeshResource->IndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

        Context->DrawIndexedInstanced(MeshResource->IndexCount, InstanceCount, 0, 0, 0);
    }
    else
    {
        // single path는 ShaderMesh.hlsl 기준:
        // cbuffer FMeshUnlitConstants { row_major float4x4 MVP; float4 BaseColor; }

        const UINT VertexStride = sizeof(FVertexSimple);
        const UINT VertexOffset = 0;

        ID3D11Buffer* VB = MeshResource->VertexBuffer.Get();
        Context->IASetVertexBuffers(0, 1, &VB, &VertexStride, &VertexOffset);
        Context->IASetIndexBuffer(MeshResource->IndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

        for (const FMeshDrawData& Draw : Draws)
        {
            FMeshUnlitConstants Constants = {};
            Constants.MVP = Draw.World * InSceneView->GetViewProjectionMatrix();
            Constants.BaseColor = Draw.Color;

            RHI->UpdateConstantBuffer(SingleConstantBuffer.Get(), &Constants, sizeof(Constants));
            Context->DrawIndexed(MeshResource->IndexCount, 0, 0);
        }
    }
}

FBasicMeshResource* FD3D11MeshBatchRenderer::GetBasicMeshResource(EBasicMeshType InType)
{
    const uint32 MeshIndex = ToMeshIndex(InType);
    if (MeshIndex >= static_cast<uint32>(EBasicMeshType::Count))
    {
        return nullptr;
    }

    return &BasicMeshResources[MeshIndex];
}

const FBasicMeshResource* FD3D11MeshBatchRenderer::GetBasicMeshResource(EBasicMeshType InType) const
{
    const uint32 MeshIndex = ToMeshIndex(InType);
    if (MeshIndex >= static_cast<uint32>(EBasicMeshType::Count))
    {
        return nullptr;
    }

    return &BasicMeshResources[MeshIndex];
}