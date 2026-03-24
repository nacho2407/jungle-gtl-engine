#include "Renderer/D3D11/D3D11FontBatchRenderer.h"

#include "Renderer/D3D11/D3D11DynamicRHI.h"
#include "Renderer/RenderAsset/FontResource.h"
#include "Renderer/SceneView.h"

#include <algorithm>
#include <functional>

namespace
{
    constexpr float DefaultLineHeight = 16.0f;

    struct FTextRenderItemLess
    {
        bool operator()(const FTextRenderItem& A, const FTextRenderItem& B) const
        {
            if (A.Priority != B.Priority)
            {
                return A.Priority < B.Priority;
            }

            if (A.Placement.Mode != B.Placement.Mode)
            {
                return static_cast<uint8>(A.Placement.Mode) < static_cast<uint8>(B.Placement.Mode);
            }

            if (A.FontResource != B.FontResource)
            {
                return std::less<const FFontResource*>{}(A.FontResource, B.FontResource);
            }

            return A.SubmissionOrder < B.SubmissionOrder;
        }
    };
}

bool FD3D11FontBatchRenderer::Initialize(FD3D11DynamicRHI* InRHI)
{
    if (InRHI == nullptr)
    {
        return false;
    }

    RHI = InRHI;
    CurrentSceneView = nullptr;
    CurrentFontResource = nullptr;
    CurrentPlacementMode = ERenderPlacementMode::World;
    NextSubmissionOrder = 0;

    Vertices.reserve(MaxVertexCount);
    Indices.reserve(MaxIndexCount);
    PendingTextItems.reserve(1024);

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

void FD3D11FontBatchRenderer::Shutdown()
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
    PendingTextItems.clear();

    CurrentFontResource = nullptr;
    CurrentSceneView = nullptr;
    CurrentPlacementMode = ERenderPlacementMode::World;
    NextSubmissionOrder = 0;
    RHI = nullptr;
}

void FD3D11FontBatchRenderer::BeginFrame() { BeginFrame(CurrentSceneView); }

void FD3D11FontBatchRenderer::BeginFrame(const FSceneView* InSceneView)
{
    CurrentSceneView = InSceneView;
    CurrentFontResource = nullptr;
    CurrentPlacementMode = ERenderPlacementMode::World;
    NextSubmissionOrder = 0;

    Vertices.clear();
    Indices.clear();
    PendingTextItems.clear();
}

void FD3D11FontBatchRenderer::AddText(const FTextRenderItem& InItem)
{
    if (RHI == nullptr || CurrentSceneView == nullptr)
    {
        return;
    }

    if (!InItem.State.IsVisible() || InItem.FontResource == nullptr || InItem.Text.empty())
    {
        return;
    }

    if (InItem.FontResource->GetSRV() == nullptr)
    {
        return;
    }

    FTextRenderItem Item = InItem;
    Item.SubmissionOrder = NextSubmissionOrder++;
    PendingTextItems.push_back(Item);
}

FVector FD3D11FontBatchRenderer::MakeScreenClipPosition(float InScreenX, float InScreenY,
                                                        float InDepth) const
{
    if (CurrentSceneView == nullptr)
    {
        return FVector::ZeroVector;
    }

    const FViewportRect& ViewRect = CurrentSceneView->GetViewRect();
    const float Width = static_cast<float>((ViewRect.Width > 0) ? ViewRect.Width : 1);
    const float Height = static_cast<float>((ViewRect.Height > 0) ? ViewRect.Height : 1);

    const float ClipX = (InScreenX / Width) * 2.0f - 1.0f;
    const float ClipY = 1.0f - (InScreenY / Height) * 2.0f;
    return FVector(ClipX, ClipY, InDepth);
}

bool FD3D11FontBatchRenderer::IsSameBatch(const FTextRenderItem& InItem) const
{
    return CurrentFontResource == InItem.FontResource &&
           CurrentPlacementMode == InItem.Placement.Mode;
}

void FD3D11FontBatchRenderer::AppendTextItem(const FTextRenderItem& InItem)
{
    if (CurrentSceneView == nullptr || InItem.FontResource == nullptr)
    {
        return;
    }

    if (!IsSameBatch(InItem) && CurrentFontResource != nullptr)
    {
        Flush(CurrentSceneView);
    }

    CurrentFontResource = InItem.FontResource;
    CurrentPlacementMode = InItem.Placement.Mode;

    const float Scale = InItem.TextScale;
    const float LetterSpacing = InItem.LetterSpacing;
    const float LineSpacing = InItem.LineSpacing;
    const float LineHeight = (InItem.FontResource->Common.LineHeight > 0)
                                 ? static_cast<float>(InItem.FontResource->Common.LineHeight)
                                 : DefaultLineHeight;

    FVector TextOrigin;
    FVector RightAxis;
    FVector UpAxis;

    if (InItem.Placement.IsScreenSpace())
    {
        const FVector2 ScreenPos = InItem.Placement.ScreenPosition;
        TextOrigin = MakeScreenClipPosition(ScreenPos.X, ScreenPos.Y, 0.0f);

        const FViewportRect& ViewRect = CurrentSceneView->GetViewRect();
        const float Width = static_cast<float>((ViewRect.Width > 0) ? ViewRect.Width : 1);
        const float Height = static_cast<float>((ViewRect.Height > 0) ? ViewRect.Height : 1);

        const float PixelToClipX = 2.0f / Width;
        const float PixelToClipY = 2.0f / Height;

        RightAxis = FVector(PixelToClipX, 0.0f, 0.0f);
        UpAxis = FVector(0.0f, PixelToClipY, 0.0f);
    }
    else
    {
        const FMatrix& PlacementWorld = InItem.Placement.World;
        TextOrigin = PlacementWorld.GetOrigin() + InItem.Placement.WorldOffset;

        if (InItem.Placement.IsBillboard())
        {
            const FMatrix CameraWorld = CurrentSceneView->GetViewMatrix().GetInverse();
            RightAxis = CameraWorld.GetRightVector();
            UpAxis = CameraWorld.GetUpVector();
        }
        else
        {
            RightAxis = PlacementWorld.GetRightVector();
            UpAxis = PlacementWorld.GetUpVector();
        }
    }

    float PenX = 0.0f;
    float PenY = 0.0f;

    for (char Ch : InItem.Text)
    {
        if (Ch == '\r')
        {
            continue;
        }

        if (Ch == '\n')
        {
            PenX = 0.0f;
            PenY += (LineHeight + LineSpacing) * Scale;
            continue;
        }

        const uint32 CodePoint = static_cast<uint8>(Ch);
        const FFontGlyph* Glyph = InItem.FontResource->FindGlyph(CodePoint);
        if (Glyph == nullptr)
        {
            PenX += Scale * (LineHeight * 0.5f + LetterSpacing);
            continue;
        }

        if (!Glyph->IsValid())
        {
            PenX += Scale * (static_cast<float>(Glyph->XAdvance) + LetterSpacing);
            continue;
        }

        if (Vertices.size() + 4 > MaxVertexCount || Indices.size() + 6 > MaxIndexCount)
        {
            Flush(CurrentSceneView);
            CurrentFontResource = InItem.FontResource;
            CurrentPlacementMode = InItem.Placement.Mode;
        }

        const float GlyphX = PenX + static_cast<float>(Glyph->XOffset) * Scale;
        const float GlyphY = PenY + static_cast<float>(Glyph->YOffset) * Scale;

        const FVector BottomLeft = TextOrigin + RightAxis * GlyphX - UpAxis * GlyphY;
        const FVector GlyphRight = RightAxis * (static_cast<float>(Glyph->Width) * Scale);
        const FVector GlyphUp = UpAxis * (static_cast<float>(Glyph->Height) * Scale);

        AppendGlyphQuad(BottomLeft, GlyphRight, GlyphUp, *Glyph, *InItem.FontResource,
                        InItem.Color);

        PenX += Scale * (static_cast<float>(Glyph->XAdvance) + LetterSpacing);
    }
}

void FD3D11FontBatchRenderer::EndFrame(const FSceneView* InSceneView)
{
    if (InSceneView == nullptr)
    {
        PendingTextItems.clear();
        Vertices.clear();
        Indices.clear();
        CurrentSceneView = nullptr;
        CurrentFontResource = nullptr;
        CurrentPlacementMode = ERenderPlacementMode::World;
        return;
    }

    CurrentSceneView = InSceneView;
    CurrentFontResource = nullptr;
    CurrentPlacementMode = ERenderPlacementMode::World;
    Vertices.clear();
    Indices.clear();

    std::stable_sort(PendingTextItems.begin(), PendingTextItems.end(), FTextRenderItemLess{});

    for (const FTextRenderItem& Item : PendingTextItems)
    {
        AppendTextItem(Item);
    }

    Flush(InSceneView);

    PendingTextItems.clear();
    CurrentSceneView = nullptr;
    CurrentFontResource = nullptr;
    CurrentPlacementMode = ERenderPlacementMode::World;
}

void FD3D11FontBatchRenderer::Flush(const FSceneView* InSceneView)
{
    if (RHI == nullptr || InSceneView == nullptr || CurrentFontResource == nullptr ||
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
                                  static_cast<uint32>(Vertices.size() * sizeof(FFontVertex))) ||
        !RHI->UpdateDynamicBuffer(DynamicIndexBuffer.Get(), Indices.data(),
                                  static_cast<uint32>(Indices.size() * sizeof(uint32))))
    {
        Vertices.clear();
        Indices.clear();
        return;
    }

    FFontConstants Constants = {};
    Constants.VP = CurrentPlacementMode == ERenderPlacementMode::Screen
                       ? FMatrix::Identity
                       : InSceneView->GetViewProjectionMatrix();
    Constants.TintColor = FColor::White();

    if (!RHI->UpdateConstantBuffer(ConstantBuffer.Get(), &Constants, sizeof(Constants)))
    {
        Vertices.clear();
        Indices.clear();
        return;
    }

    const UINT VertexStride = sizeof(FFontVertex);
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

    ID3D11ShaderResourceView* SRV = CurrentFontResource->GetSRV();
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

void FD3D11FontBatchRenderer::AppendGlyphQuad(const FVector& InBottomLeft, const FVector& InRight,
                                              const FVector& InUp, const FFontGlyph& InGlyph,
                                              const FFontResource& InFont, const FColor& InColor)
{
    const float InvW = InFont.GetInvAtlasWidth();
    const float InvH = InFont.GetInvAtlasHeight();

    const float U0 = static_cast<float>(InGlyph.X) * InvW;
    const float V0 = static_cast<float>(InGlyph.Y) * InvH;
    const float U1 = static_cast<float>(InGlyph.X + InGlyph.Width) * InvW;
    const float V1 = static_cast<float>(InGlyph.Y + InGlyph.Height) * InvH;

    const uint32 BaseVertex = static_cast<uint32>(Vertices.size());

    FFontVertex Vtx0;
    Vtx0.Position = InBottomLeft;
    Vtx0.UV = FVector2(U0, V1);
    Vtx0.Color = InColor;

    FFontVertex Vtx1;
    Vtx1.Position = InBottomLeft + InRight;
    Vtx1.UV = FVector2(U1, V1);
    Vtx1.Color = InColor;

    FFontVertex Vtx2;
    Vtx2.Position = InBottomLeft + InUp;
    Vtx2.UV = FVector2(U0, V0);
    Vtx2.Color = InColor;

    FFontVertex Vtx3;
    Vtx3.Position = InBottomLeft + InRight + InUp;
    Vtx3.UV = FVector2(U1, V0);
    Vtx3.Color = InColor;

    Vertices.push_back(Vtx0);
    Vertices.push_back(Vtx1);
    Vertices.push_back(Vtx2);
    Vertices.push_back(Vtx3);

    Indices.push_back(BaseVertex + 0);
    Indices.push_back(BaseVertex + 1);
    Indices.push_back(BaseVertex + 2);

    Indices.push_back(BaseVertex + 2);
    Indices.push_back(BaseVertex + 1);
    Indices.push_back(BaseVertex + 3);
}

bool FD3D11FontBatchRenderer::CreateShaders()
{
    if (RHI == nullptr)
    {
        return false;
    }

    static const D3D11_INPUT_ELEMENT_DESC InputElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         static_cast<UINT>(offsetof(FFontVertex, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
         static_cast<UINT>(offsetof(FFontVertex, UV)), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         static_cast<UINT>(offsetof(FFontVertex, Color)), D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    if (!RHI->CreateVertexShaderAndInputLayout(ShaderPath, "VSMain", InputElements,
                                               static_cast<uint32>(std::size(InputElements)),
                                               VertexShader.GetAddressOf(),
                                               InputLayout.GetAddressOf()))
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

bool FD3D11FontBatchRenderer::CreateConstantBuffer()
{
    return (RHI != nullptr) &&
           RHI->CreateConstantBuffer(sizeof(FFontConstants), ConstantBuffer.GetAddressOf());
}

bool FD3D11FontBatchRenderer::CreateStates()
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

bool FD3D11FontBatchRenderer::CreateBuffers()
{
    if (RHI == nullptr || RHI->GetDevice() == nullptr)
    {
        return false;
    }

    D3D11_BUFFER_DESC VertexBufferDesc = {};
    VertexBufferDesc.ByteWidth = sizeof(FFontVertex) * MaxVertexCount;
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

    return SUCCEEDED(
        RHI->GetDevice()->CreateBuffer(&IndexBufferDesc, nullptr, DynamicIndexBuffer.GetAddressOf()));
}
