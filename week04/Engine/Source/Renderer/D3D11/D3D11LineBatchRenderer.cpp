#include "Core/Geometry/Primitives/AABB.h"
#include "Renderer/D3D11/D3D11LineBatchRenderer.h"
#include "Renderer/D3D11/D3D11DynamicRHI.h"
#include "Renderer/SceneView.h"
#include "Renderer/Types/ShaderConstants.h"
#include "Renderer/Types/AxisColors.h"

bool FD3D11LineBatchRenderer::Initialize(FD3D11DynamicRHI* InRHI)
{
    RHI = InRHI;
    CurrentSceneView = nullptr;

    Vertices.reserve(MaxVertexCount);

    if (!CreateShaders())
    {
        return false;
    }

    if (!CreateConstantBuffer())
    {
        return false;
    }

    if (!CreateDynamicVertexBuffer(MaxVertexCount))
    {
        return false;
    }

    return true;
}

void FD3D11LineBatchRenderer::Shutdown()
{
    DynamicVertexBuffer.Reset();
    ConstantBuffer.Reset();
    InputLayout.Reset();
    PixelShader.Reset();
    VertexShader.Reset();

    Vertices.clear();
    CurrentSceneView = nullptr;
    RHI = nullptr;
}

void FD3D11LineBatchRenderer::BeginFrame(const FSceneView* InSceneView)
{
    CurrentSceneView = InSceneView;
    Vertices.clear();
}

void FD3D11LineBatchRenderer::AddLine(const FVector& InStart, const FVector& InEnd,
                                      const FColor& InColor)
{
    if (RHI == nullptr || CurrentSceneView == nullptr)
    {
        return;
    }

    if (Vertices.size() + 2 > MaxVertexCount)
    {
        Flush();
    }

    if (Vertices.size() + 2 > MaxVertexCount)
    {
        return;
    }

    FLineVertex V0 = {};
    V0.Position = InStart;
    V0.Color = InColor;

    FLineVertex V1 = {};
    V1.Position = InEnd;
    V1.Color = InColor;

    Vertices.push_back(V0);
    Vertices.push_back(V1);
}

void FD3D11LineBatchRenderer::AddGrid(int32 InHalfLineCount, float InSpacing,
                                      const FColor& InColor)
{
    const float Extent = static_cast<float>(InHalfLineCount) * InSpacing;

    for (int32 i = -InHalfLineCount; i <= InHalfLineCount; ++i)
    {
        const float Offset = static_cast<float>(i) * InSpacing;

        AddLine(FVector(-Extent, 0.0f, Offset), FVector(Extent, 0.0f, Offset), InColor);
        AddLine(FVector(Offset, 0.0f, -Extent), FVector(Offset, 0.0f, Extent), InColor);
    }
}

void FD3D11LineBatchRenderer::AddWorldAxes(float InAxisLength)
{
    AddLine(FVector(0.0f, 0.0f, 0.0f), FVector(InAxisLength, 0.0f, 0.0f),
            GetAxisBaseColor(EAxis::X));
    AddLine(FVector(0.0f, 0.0f, 0.0f), FVector(0.0f, InAxisLength, 0.0f),
            GetAxisBaseColor(EAxis::Y));
    AddLine(FVector(0.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, InAxisLength),
            GetAxisBaseColor(EAxis::Z));
}

void FD3D11LineBatchRenderer::AddAABB(const Geometry::FAABB& InBounds, const FColor& InColor)
{
    const FVector Min = InBounds.Min;
    const FVector Max = InBounds.Max;

    const FVector P000(Min.X, Min.Y, Min.Z);
    const FVector P100(Max.X, Min.Y, Min.Z);
    const FVector P010(Min.X, Max.Y, Min.Z);
    const FVector P110(Max.X, Max.Y, Min.Z);

    const FVector P001(Min.X, Min.Y, Max.Z);
    const FVector P101(Max.X, Min.Y, Max.Z);
    const FVector P011(Min.X, Max.Y, Max.Z);
    const FVector P111(Max.X, Max.Y, Max.Z);

    AddLine(P000, P100, InColor);
    AddLine(P100, P101, InColor);
    AddLine(P101, P001, InColor);
    AddLine(P001, P000, InColor);

    AddLine(P010, P110, InColor);
    AddLine(P110, P111, InColor);
    AddLine(P111, P011, InColor);
    AddLine(P011, P010, InColor);

    AddLine(P000, P010, InColor);
    AddLine(P100, P110, InColor);
    AddLine(P101, P111, InColor);
    AddLine(P001, P011, InColor);
}

void FD3D11LineBatchRenderer::EndFrame()
{
    Flush();
    CurrentSceneView = nullptr;
}

bool FD3D11LineBatchRenderer::CreateShaders()
{
    if (RHI == nullptr)
    {
        return false;
    }

    static const D3D11_INPUT_ELEMENT_DESC InputElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         static_cast<UINT>(offsetof(FLineVertex, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         static_cast<UINT>(offsetof(FLineVertex, Color)), D3D11_INPUT_PER_VERTEX_DATA, 0},
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
        VertexShader.Reset();
        InputLayout.Reset();
        return false;
    }

    return true;
}

bool FD3D11LineBatchRenderer::CreateConstantBuffer()
{
    if (RHI == nullptr)
    {
        return false;
    }

    return RHI->CreateConstantBuffer(sizeof(FLineConstants), ConstantBuffer.GetAddressOf());
}

bool FD3D11LineBatchRenderer::CreateDynamicVertexBuffer(uint32 InMaxVertexCount)
{
    if (RHI == nullptr)
    {
        return false;
    }

    D3D11_BUFFER_DESC Desc = {};
    Desc.ByteWidth = sizeof(FLineVertex) * InMaxVertexCount;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;
    Desc.StructureByteStride = 0;

    return SUCCEEDED(
        RHI->GetDevice()->CreateBuffer(&Desc, nullptr, DynamicVertexBuffer.GetAddressOf()));
}

void FD3D11LineBatchRenderer::Flush()
{
    if (RHI == nullptr || CurrentSceneView == nullptr)
    {
        return;
    }

    if (Vertices.empty())
    {
        return;
    }

    ID3D11DeviceContext* Context = RHI->GetDeviceContext();
    if (Context == nullptr)
    {
        return;
    }

    D3D11_MAPPED_SUBRESOURCE Mapped = {};
    HRESULT Hr = Context->Map(DynamicVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
    if (FAILED(Hr))
    {
        return;
    }

    memcpy(Mapped.pData, Vertices.data(), sizeof(FLineVertex) * Vertices.size());
    Context->Unmap(DynamicVertexBuffer.Get(), 0);

    FLineConstants Constants = {};
    Constants.VP = CurrentSceneView->GetViewProjectionMatrix();

    RHI->UpdateConstantBuffer(ConstantBuffer.Get(), &Constants, sizeof(Constants));

    const UINT    Stride = sizeof(FLineVertex);
    const UINT    Offset = 0;
    ID3D11Buffer* VertexBuffer = DynamicVertexBuffer.Get();

    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    Context->IASetInputLayout(InputLayout.Get());
    Context->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);

    Context->VSSetShader(VertexShader.Get(), nullptr, 0);
    Context->VSSetConstantBuffers(0, 1, ConstantBuffer.GetAddressOf());

    Context->PSSetShader(PixelShader.Get(), nullptr, 0);

    Context->Draw(static_cast<UINT>(Vertices.size()), 0);

    Vertices.clear();
}