#include "Renderer/D3D11/D3D11SpriteBatchRenderer.h"

#include "Renderer/D3D11/D3D11DynamicRHI.h"
#include "Renderer/RenderAsset/TextureResource.h"
#include "Renderer/SceneView.h"

#include <algorithm>
#include <functional>

namespace
{
    struct FSpriteRenderItemLess
    {
        bool operator()(const FSpriteRenderItem& A, const FSpriteRenderItem& B) const
        {
            if (A.Priority != B.Priority)
            {
                return A.Priority < B.Priority;
            }

            if (A.Placement.Mode != B.Placement.Mode)
            {
                return static_cast<uint8>(A.Placement.Mode) < static_cast<uint8>(B.Placement.Mode);
            }

            if (A.TextureResource != B.TextureResource)
            {
                return std::less<const FTextureResource*>{}(A.TextureResource, B.TextureResource);
            }

            return A.SubmissionOrder < B.SubmissionOrder;
        }
    };
} // namespace

bool FD3D11SpriteBatchRenderer::Initialize(FD3D11DynamicRHI* InRHI)
{
    if (InRHI == nullptr)
    {
        return false;
    }

    RHI = InRHI;
    CurrentSceneView = nullptr;
    CurrentTextureResource = nullptr;
    CurrentPlacementMode = ERenderPlacementMode::World;
    NextSubmissionOrder = 0;

    Vertices.reserve(MaxVertexCount);
    Indices.reserve(MaxIndexCount);
    PendingSpriteItems.reserve(2048);

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

    if (!CreateBuffers())
    {
        Shutdown();
        return false;
    }

    return true;
}

void FD3D11SpriteBatchRenderer::Shutdown()
{
    RasterizerState.Reset();
    ScreenDepthStencilState.Reset();
    DepthStencilState.Reset();
    AlphaBlendState.Reset();
    SamplerState.Reset();

    DynamicIndexBuffer.Reset();
    DynamicVertexBuffer.Reset();
    ConstantBuffer.Reset();

    InputLayout.Reset();
    PixelShader.Reset();
    VertexShader.Reset();

    Vertices.clear();
    Indices.clear();
    PendingSpriteItems.clear();

    CurrentTextureResource = nullptr;
    CurrentSceneView = nullptr;
    CurrentPlacementMode = ERenderPlacementMode::World;
    NextSubmissionOrder = 0;
    RHI = nullptr;
}

void FD3D11SpriteBatchRenderer::BeginFrame() { BeginFrame(CurrentSceneView); }

void FD3D11SpriteBatchRenderer::BeginFrame(const FSceneView* InSceneView)
{
    CurrentSceneView = InSceneView;
    CurrentTextureResource = nullptr;
    CurrentPlacementMode = ERenderPlacementMode::World;
    NextSubmissionOrder = 0;

    Vertices.clear();
    Indices.clear();
    PendingSpriteItems.clear();
}

void FD3D11SpriteBatchRenderer::AddSprite(const FSpriteRenderItem& InItem)
{
    if (RHI == nullptr || CurrentSceneView == nullptr)
    {
        return;
    }

    if (!InItem.State.IsVisible() || InItem.TextureResource == nullptr)
    {
        return;
    }

    if (InItem.TextureResource->GetSRV() == nullptr)
    {
        return;
    }

    FSpriteRenderItem Item = InItem;
    Item.SubmissionOrder = NextSubmissionOrder++;
    PendingSpriteItems.push_back(Item);
}

void FD3D11SpriteBatchRenderer::AddSprites(const TArray<FSpriteRenderItem>& InItems)
{
    for (const FSpriteRenderItem& Item : InItems)
    {
        AddSprite(Item);
    }
}

FVector FD3D11SpriteBatchRenderer::MakeScreenClipPosition(float InScreenX, float InScreenY,
                                                          float InDepth) const
{
    if (CurrentSceneView == nullptr)
    {
        return FVector::ZeroVector;
    }

    const FViewportRect& ViewRect = CurrentSceneView->GetViewRect();
    const float          Width = static_cast<float>((ViewRect.Width > 0) ? ViewRect.Width : 1);
    const float          Height = static_cast<float>((ViewRect.Height > 0) ? ViewRect.Height : 1);

    const float ClipX = (InScreenX / Width) * 2.0f - 1.0f;
    const float ClipY = 1.0f - (InScreenY / Height) * 2.0f;
    return FVector(ClipX, ClipY, InDepth);
}

bool FD3D11SpriteBatchRenderer::IsSameBatch(const FSpriteRenderItem& InItem) const
{
    return CurrentTextureResource == InItem.TextureResource &&
           CurrentPlacementMode == InItem.Placement.Mode;
}

void FD3D11SpriteBatchRenderer::AppendQuad(const FVector& InBottomLeft, const FVector& InRight,
                                           const FVector& InUp, const FVector2& InUVMin,
                                           const FVector2& InUVMax, const FColor& InColor)
{
    const uint32 BaseVertex = static_cast<uint32>(Vertices.size());

    FSpriteVertex V0 = {};
    V0.Position = InBottomLeft;
    V0.UV = FVector2(InUVMin.X, InUVMax.Y);
    V0.Color = InColor;

    FSpriteVertex V1 = {};
    V1.Position = InBottomLeft + InUp;
    V1.UV = FVector2(InUVMin.X, InUVMin.Y);
    V1.Color = InColor;

    FSpriteVertex V2 = {};
    V2.Position = InBottomLeft + InRight + InUp;
    V2.UV = FVector2(InUVMax.X, InUVMin.Y);
    V2.Color = InColor;

    FSpriteVertex V3 = {};
    V3.Position = InBottomLeft + InRight;
    V3.UV = FVector2(InUVMax.X, InUVMax.Y);
    V3.Color = InColor;

    Vertices.push_back(V0);
    Vertices.push_back(V1);
    Vertices.push_back(V2);
    Vertices.push_back(V3);

    Indices.push_back(BaseVertex + 0);
    Indices.push_back(BaseVertex + 1);
    Indices.push_back(BaseVertex + 2);
    Indices.push_back(BaseVertex + 0);
    Indices.push_back(BaseVertex + 2);
    Indices.push_back(BaseVertex + 3);
}

void FD3D11SpriteBatchRenderer::AppendSpriteItem(const FSpriteRenderItem& InItem)
{
    if (CurrentSceneView == nullptr || InItem.TextureResource == nullptr)
    {
        return;
    }

    if (!IsSameBatch(InItem) && CurrentTextureResource != nullptr)
    {
        Flush(CurrentSceneView);
    }

    CurrentTextureResource = InItem.TextureResource;
    CurrentPlacementMode = InItem.Placement.Mode;

    if (Vertices.size() + 4 > MaxVertexCount || Indices.size() + 6 > MaxIndexCount)
    {
        Flush(CurrentSceneView);
        CurrentTextureResource = InItem.TextureResource;
        CurrentPlacementMode = InItem.Placement.Mode;
    }

    FVector RightAxis;
    FVector UpAxis;
    FVector BottomLeft;

    if (InItem.Placement.IsScreenSpace())
    {
        const FVector2 ScreenPos = InItem.Placement.ScreenPosition;
        const FVector  WorldScale = InItem.Placement.World.GetScaleVector();
        const float    HalfWidth = WorldScale.X;
        const float    HalfHeight = WorldScale.Z;

        BottomLeft =
            MakeScreenClipPosition(ScreenPos.X - HalfWidth, ScreenPos.Y + HalfHeight, 0.0f);
        const FVector BottomRight =
            MakeScreenClipPosition(ScreenPos.X + HalfWidth, ScreenPos.Y + HalfHeight, 0.0f);
        const FVector TopLeft =
            MakeScreenClipPosition(ScreenPos.X - HalfWidth, ScreenPos.Y - HalfHeight, 0.0f);

        RightAxis = BottomRight - BottomLeft;
        UpAxis = TopLeft - BottomLeft;
    }
    else
    {
        const FMatrix& PlacementWorld = InItem.Placement.World;
        FVector        SpriteOrigin = PlacementWorld.GetOrigin() + InItem.Placement.WorldOffset;

        if (InItem.Placement.IsBillboard())
        {
            const FMatrix CameraWorld = CurrentSceneView->GetViewMatrix().GetInverse();
            RightAxis = CameraWorld.GetRightVector();
            UpAxis = CameraWorld.GetUpVector();

            const FVector WorldScale = PlacementWorld.GetScaleVector();
            RightAxis = RightAxis * WorldScale.X;
            UpAxis = UpAxis * WorldScale.Z;
        }
        else
        {
            RightAxis = PlacementWorld.GetRightVector();
            UpAxis = PlacementWorld.GetUpVector();
        }

        BottomLeft = SpriteOrigin - RightAxis - UpAxis;
        RightAxis = RightAxis * 2.0f;
        UpAxis = UpAxis * 2.0f;
    }

    AppendQuad(BottomLeft, RightAxis, UpAxis, InItem.UVMin, InItem.UVMax, InItem.Color);
}

void FD3D11SpriteBatchRenderer::EndFrame(const FSceneView* InSceneView)
{
    if (InSceneView == nullptr)
    {
        PendingSpriteItems.clear();
        Vertices.clear();
        Indices.clear();
        CurrentSceneView = nullptr;
        CurrentTextureResource = nullptr;
        CurrentPlacementMode = ERenderPlacementMode::World;
        return;
    }

    CurrentSceneView = InSceneView;
    CurrentTextureResource = nullptr;
    CurrentPlacementMode = ERenderPlacementMode::World;
    Vertices.clear();
    Indices.clear();

    std::sort(PendingSpriteItems.begin(), PendingSpriteItems.end(), FSpriteRenderItemLess{});

    for (const FSpriteRenderItem& Item : PendingSpriteItems)
    {
        AppendSpriteItem(Item);
    }

    Flush(InSceneView);

    PendingSpriteItems.clear();
    CurrentSceneView = nullptr;
    CurrentTextureResource = nullptr;
    CurrentPlacementMode = ERenderPlacementMode::World;
}

void FD3D11SpriteBatchRenderer::Flush(const FSceneView* InSceneView)
{
    if (RHI == nullptr || InSceneView == nullptr || CurrentTextureResource == nullptr ||
        Vertices.empty() || Indices.empty())
    {
        Vertices.clear();
        Indices.clear();
        return;
    }

    ID3D11DeviceContext* Context = RHI->GetDeviceContext();
    if (Context == nullptr)
    {
        Vertices.clear();
        Indices.clear();
        return;
    }

    if (!RHI->UpdateDynamicBuffer(DynamicVertexBuffer.Get(), Vertices.data(),
                                  static_cast<uint32>(Vertices.size() * sizeof(FSpriteVertex))) ||
        !RHI->UpdateDynamicBuffer(DynamicIndexBuffer.Get(), Indices.data(),
                                  static_cast<uint32>(Indices.size() * sizeof(uint32))))
    {
        Vertices.clear();
        Indices.clear();
        return;
    }

    FSpriteConstants Constants = {};
    Constants.VP = CurrentPlacementMode == ERenderPlacementMode::Screen
                       ? FMatrix::Identity
                       : InSceneView->GetViewProjectionMatrix();

    if (!RHI->UpdateConstantBuffer(ConstantBuffer.Get(), &Constants, sizeof(Constants)))
    {
        Vertices.clear();
        Indices.clear();
        return;
    }

    const UINT VertexStride = sizeof(FSpriteVertex);
    const UINT VertexOffset = 0;

    RHI->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    RHI->SetInputLayout(InputLayout.Get());
    RHI->SetVertexShader(VertexShader.Get());
    RHI->SetPixelShader(PixelShader.Get());
    RHI->SetVSConstantBuffer(0, ConstantBuffer.Get());
    RHI->SetPSConstantBuffer(0, ConstantBuffer.Get());
    RHI->SetVertexBuffer(0, DynamicVertexBuffer.Get(), VertexStride, VertexOffset);
    RHI->SetIndexBuffer(DynamicIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    RHI->SetRasterizerState(RasterizerState.Get());
    RHI->SetDepthStencilState(CurrentPlacementMode == ERenderPlacementMode::Screen
                                  ? ScreenDepthStencilState.Get()
                                  : DepthStencilState.Get(),
                              0);

    const float BlendFactor[4] = {0.f, 0.f, 0.f, 0.f};
    Context->OMSetBlendState(AlphaBlendState.Get(), BlendFactor, 0xFFFFFFFFu);

    ID3D11ShaderResourceView* SRV = CurrentTextureResource->GetSRV();
    Context->PSSetShaderResources(0, 1, &SRV);

    ID3D11SamplerState* Sampler = SamplerState.Get();
    Context->PSSetSamplers(0, 1, &Sampler);

    RHI->DrawIndexed(static_cast<uint32>(Indices.size()), 0, 0);

    ID3D11ShaderResourceView* NullSRV = nullptr;
    Context->PSSetShaderResources(0, 1, &NullSRV);
    Context->OMSetBlendState(nullptr, BlendFactor, 0xFFFFFFFFu);

    Vertices.clear();
    Indices.clear();
}

bool FD3D11SpriteBatchRenderer::CreateShaders()
{
    if (RHI == nullptr)
    {
        return false;
    }

    static const D3D11_INPUT_ELEMENT_DESC InputElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         static_cast<UINT>(offsetof(FSpriteVertex, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(FSpriteVertex, UV)),
         D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         static_cast<UINT>(offsetof(FSpriteVertex, Color)), D3D11_INPUT_PER_VERTEX_DATA, 0},
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

bool FD3D11SpriteBatchRenderer::CreateConstantBuffer()
{
    return (RHI != nullptr) &&
           RHI->CreateConstantBuffer(sizeof(FSpriteConstants), ConstantBuffer.GetAddressOf());
}

bool FD3D11SpriteBatchRenderer::CreateStates()
{
    if (RHI == nullptr)
    {
        return false;
    }

    D3D11_SAMPLER_DESC SamplerDesc = {};
    SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SamplerDesc.MinLOD = 0.0f;
    SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    if (!RHI->CreateSamplerState(SamplerDesc, SamplerState.GetAddressOf()))
    {
        return false;
    }

    D3D11_BLEND_DESC BlendDesc = {};
    BlendDesc.RenderTarget[0].BlendEnable = TRUE;
    BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    if (!RHI->CreateBlendState(BlendDesc, AlphaBlendState.GetAddressOf()))
    {
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC DepthDesc = {};
    DepthDesc.DepthEnable = TRUE;
    DepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    DepthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

    if (!RHI->CreateDepthStencilState(DepthDesc, DepthStencilState.GetAddressOf()))
    {
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC ScreenDepthDesc = {};
    ScreenDepthDesc.DepthEnable = FALSE;
    ScreenDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    ScreenDepthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;

    if (!RHI->CreateDepthStencilState(ScreenDepthDesc, ScreenDepthStencilState.GetAddressOf()))
    {
        return false;
    }

    D3D11_RASTERIZER_DESC RasterizerDesc = {};
    RasterizerDesc.FillMode = D3D11_FILL_SOLID;
    RasterizerDesc.CullMode = D3D11_CULL_NONE;
    RasterizerDesc.DepthClipEnable = TRUE;
    RasterizerDesc.ScissorEnable = FALSE;
    RasterizerDesc.MultisampleEnable = FALSE;
    RasterizerDesc.AntialiasedLineEnable = FALSE;

    return RHI->CreateRasterizerState(RasterizerDesc, RasterizerState.GetAddressOf());
}

bool FD3D11SpriteBatchRenderer::CreateBuffers()
{
    if (RHI == nullptr || RHI->GetDevice() == nullptr)
    {
        return false;
    }

    D3D11_BUFFER_DESC VertexBufferDesc = {};
    VertexBufferDesc.ByteWidth = sizeof(FSpriteVertex) * MaxVertexCount;
    VertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    VertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(RHI->GetDevice()->CreateBuffer(&VertexBufferDesc, nullptr,
                                              DynamicVertexBuffer.GetAddressOf())))
    {
        return false;
    }

    D3D11_BUFFER_DESC IndexBufferDesc = {};
    IndexBufferDesc.ByteWidth = sizeof(uint32) * MaxIndexCount;
    IndexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    IndexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    return SUCCEEDED(RHI->GetDevice()->CreateBuffer(&IndexBufferDesc, nullptr,
                                                    DynamicIndexBuffer.GetAddressOf()));
}
