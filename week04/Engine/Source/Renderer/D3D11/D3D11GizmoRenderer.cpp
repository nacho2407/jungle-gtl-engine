#include "Renderer/D3D11/D3D11GizmoRenderer.h"

#include "Renderer/D3D11/D3D11DynamicRHI.h"
#include "Renderer/SceneView.h"
#include "Renderer/Types/AxisColors.h"
#include "Renderer/Types/ShaderConstants.h"

#include "Resources/Mesh/Gizmo/GizmoRotation.h"
#include "Resources/Mesh/Gizmo/GizmoScaling.h"
#include "Resources/Mesh/Gizmo/GizmoTranslation.h"

bool FD3D11GizmoRenderer::Initialize(FD3D11DynamicRHI* InRHI)
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

    if (!CreateGizmoMeshes())
    {
        Shutdown();
        return false;
    }

    return true;
}

void FD3D11GizmoRenderer::Shutdown()
{
    ReleaseGizmoMeshes();

    DepthStencilState.Reset();
    SolidRasterizerState.Reset();

    ConstantBuffer.Reset();
    InputLayout.Reset();
    PixelShader.Reset();
    VertexShader.Reset();

    RHI = nullptr;
}

void FD3D11GizmoRenderer::Render(const FEditorRenderData& InEditorRenderData)
{
    if (RHI == nullptr)
    {
        return;
    }

    if (!InEditorRenderData.bShowGizmo)
    {
        return;
    }

    const FGizmoDrawData& Gizmo = InEditorRenderData.Gizmo;
    const FSceneView*     SceneView = InEditorRenderData.SceneView;

    if (!Gizmo.bVisible || SceneView == nullptr)
    {
        return;
    }

    if (Gizmo.GizmoType == EGizmoType::None || Gizmo.GizmoType == EGizmoType::Count)
    {
        return;
    }

    BindPipeline();

    switch (Gizmo.GizmoType)
    {
    case EGizmoType::Translation:
        DrawTranslation(Gizmo, SceneView);
        break;

    case EGizmoType::Rotation:
        DrawRotation(Gizmo, SceneView);
        break;

    case EGizmoType::Scaling:
        DrawScaling(Gizmo, SceneView);
        break;

    case EGizmoType::None:
    case EGizmoType::Count:
    default:
        break;
    }
}

bool FD3D11GizmoRenderer::CreateShaders()
{
    if (RHI == nullptr)
    {
        return false;
    }

    const D3D11_INPUT_ELEMENT_DESC InputElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    if (!RHI->CreateVertexShaderAndInputLayout(
            DefaultShaderPath, "VSMain", InputElements, static_cast<uint32>(SIZE_T(InputElements)),
            VertexShader.GetAddressOf(), InputLayout.GetAddressOf()))
    {
        return false;
    }

    if (!RHI->CreatePixelShader(DefaultShaderPath, "PSMain", PixelShader.GetAddressOf()))
    {
        return false;
    }

    return true;
}

bool FD3D11GizmoRenderer::CreateConstantBuffer()
{
    if (RHI == nullptr)
    {
        return false;
    }

    return RHI->CreateConstantBuffer(sizeof(FMeshUnlitConstants), ConstantBuffer.GetAddressOf());
}

bool FD3D11GizmoRenderer::CreateStates()
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

bool FD3D11GizmoRenderer::CreateGizmoMeshes()
{
    ReleaseGizmoMeshes();

    if (!CreateTranslationMesh(GizmoMeshes[static_cast<uint8>(EGizmoType::Translation)]))
    {
        return false;
    }

    if (!CreateRotationMesh(GizmoMeshes[static_cast<uint8>(EGizmoType::Rotation)]))
    {
        return false;
    }

    if (!CreateScalingMesh(GizmoMeshes[static_cast<uint8>(EGizmoType::Scaling)]))
    {
        return false;
    }

    return true;
}

void FD3D11GizmoRenderer::ReleaseGizmoMeshes()
{
    for (uint32 i = 0; i < static_cast<uint32>(EGizmoType::Count); ++i)
    {
        GizmoMeshes[i].VertexBuffer = nullptr;
        GizmoMeshes[i].IndexBuffer = nullptr;
        GizmoMeshes[i].IndexCount = 0;
    }
}

bool FD3D11GizmoRenderer::CreateTranslationMesh(FGizmoMeshResource& OutResource)
{
    return CreateMeshResource(gizmo_translation_vertices, gizmo_translation_vertex_count,
                              gizmo_translation_indices, gizmo_translation_index_count,
                              OutResource);
}

bool FD3D11GizmoRenderer::CreateRotationMesh(FGizmoMeshResource& OutResource)
{
    return CreateMeshResource(gizmo_rotation_vertices, gizmo_rotation_vertex_count,
                              gizmo_rotation_indices, gizmo_rotation_index_count, OutResource);
}

bool FD3D11GizmoRenderer::CreateScalingMesh(FGizmoMeshResource& OutResource)
{
    return CreateMeshResource(gizmo_scaling_vertices, gizmo_scaling_vertex_count,
                              gizmo_scaling_indices, gizmo_scaling_index_count, OutResource);
}

bool FD3D11GizmoRenderer::CreateMeshResource(const FVertexSimple* InVertices, uint32 InVertexCount,
                                             const uint16* InIndices, uint32 InIndexCount,
                                             FGizmoMeshResource& OutResource)
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

    {
        D3D11_BUFFER_DESC Desc = {};
        Desc.ByteWidth = sizeof(FVertexSimple) * InVertexCount;
        Desc.Usage = D3D11_USAGE_IMMUTABLE;
        Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA Data = {};
        Data.pSysMem = InVertices;

        if (FAILED(Device->CreateBuffer(&Desc, &Data, OutResource.VertexBuffer.GetAddressOf())))
        {
            return false;
        }
    }

    {
        D3D11_BUFFER_DESC Desc = {};
        Desc.ByteWidth = sizeof(uint16) * InIndexCount;
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

void FD3D11GizmoRenderer::BindPipeline()
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

    Context->RSSetState(SolidRasterizerState.Get());
    Context->OMSetDepthStencilState(DepthStencilState.Get(), 0);
}

void FD3D11GizmoRenderer::DrawTranslation(const FGizmoDrawData& InGizmo,
                                          const FSceneView*     InSceneView)
{
    const FGizmoMeshResource& Mesh = GizmoMeshes[static_cast<uint8>(EGizmoType::Translation)];

    DrawAxisMesh(Mesh, MakeAxisRotationX() * InGizmo.Transform, InSceneView,
                 GetAxisColor(EAxis::X, InGizmo.Highlight));

    DrawAxisMesh(Mesh, MakeAxisRotationY() * InGizmo.Transform, InSceneView,
                 GetAxisColor(EAxis::Y, InGizmo.Highlight));

    DrawAxisMesh(Mesh, MakeAxisRotationZ() * InGizmo.Transform, InSceneView,
                 GetAxisColor(EAxis::Z, InGizmo.Highlight));
}

void FD3D11GizmoRenderer::DrawRotation(const FGizmoDrawData& InGizmo, const FSceneView* InSceneView)
{
    const FGizmoMeshResource& Mesh = GizmoMeshes[static_cast<uint8>(EGizmoType::Rotation)];

    DrawAxisMesh(Mesh, MakeAxisRotationX() * InGizmo.Transform, InSceneView,
                 GetAxisColor(EAxis::X, InGizmo.Highlight));

    DrawAxisMesh(Mesh, MakeAxisRotationY() * InGizmo.Transform, InSceneView,
                 GetAxisColor(EAxis::Y, InGizmo.Highlight));

    DrawAxisMesh(Mesh, MakeAxisRotationZ() * InGizmo.Transform, InSceneView,
                 GetAxisColor(EAxis::Z, InGizmo.Highlight));
}

void FD3D11GizmoRenderer::DrawScaling(const FGizmoDrawData& InGizmo, const FSceneView* InSceneView)
{
    const FGizmoMeshResource& Mesh = GizmoMeshes[static_cast<uint8>(EGizmoType::Scaling)];

    DrawAxisMesh(Mesh, MakeAxisRotationX() * InGizmo.Transform, InSceneView,
                 GetAxisColor(EAxis::X, InGizmo.Highlight));

    DrawAxisMesh(Mesh, MakeAxisRotationY() * InGizmo.Transform, InSceneView,
                 GetAxisColor(EAxis::Y, InGizmo.Highlight));

    DrawAxisMesh(Mesh, MakeAxisRotationZ() * InGizmo.Transform, InSceneView,
                 GetAxisColor(EAxis::Z, InGizmo.Highlight));
}

void FD3D11GizmoRenderer::DrawAxisMesh(const FGizmoMeshResource& InMesh, const FMatrix& InWorld,
                                       const FSceneView* InSceneView, const FColor& InColor)
{
    if (RHI == nullptr || InSceneView == nullptr)
    {
        return;
    }

    ID3D11DeviceContext* Context = RHI->GetDeviceContext();
    if (Context == nullptr || InMesh.VertexBuffer == nullptr || InMesh.IndexBuffer == nullptr ||
        InMesh.IndexCount == 0)
    {
        return;
    }

    FMeshUnlitConstants Constants = {};
    Constants.MVP = InWorld * InSceneView->GetViewProjectionMatrix();
    Constants.BaseColor = InColor;

    Context->UpdateSubresource(ConstantBuffer.Get(), 0, nullptr, &Constants, 0, 0);

    const UINT Stride = sizeof(FVertexSimple);
    const UINT Offset = 0;

    ID3D11Buffer* VertexBuffer = InMesh.VertexBuffer.Get();
    Context->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
    Context->IASetIndexBuffer(InMesh.IndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

    Context->DrawIndexed(InMesh.IndexCount, 0, 0);
}

FColor FD3D11GizmoRenderer::GetAxisColor(EAxis InAxis, EGizmoHighlight InHighlight) const
{
    switch (InHighlight)
    {
    case EGizmoHighlight::X:
        return (InAxis == EAxis::X) ? GetAxisHighlightColor(InAxis) : GetAxisBaseColor(InAxis);

    case EGizmoHighlight::Y:
        return (InAxis == EAxis::Y) ? GetAxisHighlightColor(InAxis) : GetAxisBaseColor(InAxis);

    case EGizmoHighlight::Z:
        return (InAxis == EAxis::Z) ? GetAxisHighlightColor(InAxis) : GetAxisBaseColor(InAxis);

    case EGizmoHighlight::XY:
        return (InAxis == EAxis::X || InAxis == EAxis::Y) ? GetAxisHighlightColor(InAxis)
                                                          : GetAxisBaseColor(InAxis);

    case EGizmoHighlight::YZ:
        return (InAxis == EAxis::Y || InAxis == EAxis::Z) ? GetAxisHighlightColor(InAxis)
                                                          : GetAxisBaseColor(InAxis);

    case EGizmoHighlight::ZX:
        return (InAxis == EAxis::Z || InAxis == EAxis::X) ? GetAxisHighlightColor(InAxis)
                                                          : GetAxisBaseColor(InAxis);

    case EGizmoHighlight::XYZ:
        return GetAxisHighlightColor(InAxis);

    case EGizmoHighlight::None:
    default:
        return GetAxisBaseColor(InAxis);
    }
}

FMatrix FD3D11GizmoRenderer::MakeAxisRotationX() const { return FMatrix::Identity; }

FMatrix FD3D11GizmoRenderer::MakeAxisRotationY() const
{
    return FMatrix(0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                   0.0f, 0.0f, 1.0f);
}

FMatrix FD3D11GizmoRenderer::MakeAxisRotationZ() const
{
    return FMatrix(0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                   0.0f, 0.0f, 1.0f);
}
