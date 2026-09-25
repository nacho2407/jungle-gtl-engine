#include "Renderer/D3D11/D3D11SelectionMaskRenderer.h"

#include "Renderer/D3D11/D3D11RHI.h"
#include "Renderer/MemoryTracker.h"
#include "Renderer/RenderAsset/FontResource.h"
#include "Renderer/RenderAsset/StaticMeshResource.h"
#include "Renderer/RenderAsset/TextureResource.h"
#include "Renderer/SceneView.h"

#include <algorithm>
#include <functional>

namespace
{
    constexpr float DefaultLineHeight = 16.0f;
    constexpr uint32 Utf8ReplacementCodePoint = static_cast<uint32>('?');

    TArray<uint32> DecodeUtf8CodePoints(const FString& InText)
    {
        TArray<uint32> CodePoints;
        CodePoints.reserve(InText.size());

        const uint8* Bytes = reinterpret_cast<const uint8*>(InText.data());
        const size_t ByteCount = InText.size();

        size_t Index = 0;
        while (Index < ByteCount)
        {
            const uint8 Lead = Bytes[Index];

            if (Lead <= 0x7F)
            {
                CodePoints.push_back(static_cast<uint32>(Lead));
                ++Index;
                continue;
            }

            uint32 CodePoint = Utf8ReplacementCodePoint;
            size_t SequenceLength = 1;

            if ((Lead & 0xE0) == 0xC0)
            {
                SequenceLength = 2;
                if (Index + 1 < ByteCount && (Bytes[Index + 1] & 0xC0) == 0x80)
                {
                    const uint32 Candidate =
                        ((static_cast<uint32>(Lead & 0x1F)) << 6) |
                        static_cast<uint32>(Bytes[Index + 1] & 0x3F);

                    if (Candidate >= 0x80)
                    {
                        CodePoint = Candidate;
                    }
                }
            }
            else if ((Lead & 0xF0) == 0xE0)
            {
                SequenceLength = 3;
                if (Index + 2 < ByteCount && (Bytes[Index + 1] & 0xC0) == 0x80 &&
                    (Bytes[Index + 2] & 0xC0) == 0x80)
                {
                    const uint32 Candidate =
                        ((static_cast<uint32>(Lead & 0x0F)) << 12) |
                        ((static_cast<uint32>(Bytes[Index + 1] & 0x3F)) << 6) |
                        static_cast<uint32>(Bytes[Index + 2] & 0x3F);

                    if (Candidate >= 0x800 && !(Candidate >= 0xD800 && Candidate <= 0xDFFF))
                    {
                        CodePoint = Candidate;
                    }
                }
            }
            else if ((Lead & 0xF8) == 0xF0)
            {
                SequenceLength = 4;
                if (Index + 3 < ByteCount && (Bytes[Index + 1] & 0xC0) == 0x80 &&
                    (Bytes[Index + 2] & 0xC0) == 0x80 && (Bytes[Index + 3] & 0xC0) == 0x80)
                {
                    const uint32 Candidate =
                        ((static_cast<uint32>(Lead & 0x07)) << 18) |
                        ((static_cast<uint32>(Bytes[Index + 1] & 0x3F)) << 12) |
                        ((static_cast<uint32>(Bytes[Index + 2] & 0x3F)) << 6) |
                        static_cast<uint32>(Bytes[Index + 3] & 0x3F);

                    if (Candidate >= 0x10000 && Candidate <= 0x10FFFF)
                    {
                        CodePoint = Candidate;
                    }
                }
            }

            CodePoints.push_back(CodePoint);
            Index += SequenceLength;
        }

        return CodePoints;
    }

    float ComputePlacementDepth(const FSceneView* InSceneView, const FRenderPlacement& InPlacement)
    {
        if (InSceneView == nullptr)
        {
            return 0.0f;
        }

        const FMatrix CameraWorld = InSceneView->GetViewMatrix().GetInverse();
        const FVector CameraOrigin = CameraWorld.GetOrigin();
        const FVector CameraForward = CameraWorld.GetForwardVector();
        const FVector WorldOrigin = InPlacement.World.GetOrigin() + InPlacement.WorldOffset;
        const FVector Delta = WorldOrigin - CameraOrigin;
        return Delta.X * CameraForward.X + Delta.Y * CameraForward.Y + Delta.Z * CameraForward.Z;
    }

    struct FSpriteRenderItemLess
    {
        const FSceneView* SceneView = nullptr;

        bool operator()(const FSpriteRenderItem& A, const FSpriteRenderItem& B) const
        {
            const float DepthA = ComputePlacementDepth(SceneView, A.Placement);
            const float DepthB = ComputePlacementDepth(SceneView, B.Placement);

            if (DepthA != DepthB)
            {
                return DepthA > DepthB;
            }

            if (A.Placement.Mode != B.Placement.Mode)
            {
                return static_cast<uint8>(A.Placement.Mode) < static_cast<uint8>(B.Placement.Mode);
            }

            return std::less<const FTextureResource*>()(A.TextureResource, B.TextureResource);
        }
    };

    struct FTextRenderItemLess
    {
        const FSceneView* SceneView = nullptr;

        bool operator()(const FTextRenderItem& A, const FTextRenderItem& B) const
        {
            const float DepthA = ComputePlacementDepth(SceneView, A.Placement);
            const float DepthB = ComputePlacementDepth(SceneView, B.Placement);

            if (DepthA != DepthB)
            {
                return DepthA > DepthB;
            }

            if (A.Placement.Mode != B.Placement.Mode)
            {
                return static_cast<uint8>(A.Placement.Mode) < static_cast<uint8>(B.Placement.Mode);
            }

            return std::less<const FFontResource*>()(A.FontResource, B.FontResource);
        }
    };
}

bool FD3D11SelectionMaskRenderer::Initialize(FD3D11RHI* InRHI)
{
    if (InRHI == nullptr)
    {
        return false;
    }

    RHI = InRHI;
    CurrentSceneView = nullptr;
    bEnableOcclusion = true;

    PendingStaticMeshes.reserve(256);
    PendingSprites.reserve(1024);
    PendingTexts.reserve(1024);
    SpriteVertices.reserve(MaxVertexCount);
    SpriteIndices.reserve(MaxIndexCount);
    TextVertices.reserve(MaxVertexCount);
    TextIndices.reserve(MaxIndexCount);

    if (!CreateStaticMeshShaders())
    {
        Shutdown();
        return false;
    }

    if (!CreateSpriteShaders())
    {
        Shutdown();
        return false;
    }

    if (!CreateTextShaders())
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

    if (!CreateDynamicBuffers())
    {
        Shutdown();
        return false;
    }

    if (!CreateFallbackWhiteTexture())
    {
        Shutdown();
        return false;
    }

    if (!CreateMaskResources(RHI->GetViewportWidth(), RHI->GetViewportHeight()))
    {
        Shutdown();
        return false;
    }

    return true;
}

void FD3D11SelectionMaskRenderer::Shutdown()
{
    ReleaseMaskResources();
    ActiveDepthTarget.Reset();

    FallbackWhiteSRV.Reset();
    FallbackWhiteTexture.Reset();

    RasterizerState.Reset();
    DepthDisabledState.Reset();
    DepthEnabledState.Reset();
    AlphaBlendState.Reset();
    SamplerState.Reset();

    GMemoryTracker.RemoveIndexBufferBytes(DynamicTextIndexBuffer.Get());
    GMemoryTracker.RemoveVertexBufferBytes(DynamicTextVertexBuffer.Get());
    GMemoryTracker.RemoveIndexBufferBytes(DynamicSpriteIndexBuffer.Get());
    GMemoryTracker.RemoveVertexBufferBytes(DynamicSpriteVertexBuffer.Get());
    DynamicTextIndexBuffer.Reset();
    DynamicTextVertexBuffer.Reset();
    DynamicSpriteIndexBuffer.Reset();
    DynamicSpriteVertexBuffer.Reset();

    TextConstantBuffer.Reset();
    SpriteConstantBuffer.Reset();
    MeshConstantBuffer.Reset();

    TextInputLayout.Reset();
    TextPixelShader.Reset();
    TextVertexShader.Reset();

    SpriteInputLayout.Reset();
    SpritePixelShader.Reset();
    SpriteVertexShader.Reset();

    StaticMeshInputLayout.Reset();
    StaticMeshPixelShader.Reset();
    StaticMeshVertexShader.Reset();

    PendingStaticMeshes.clear();
    PendingSprites.clear();
    PendingTexts.clear();
    SpriteVertices.clear();
    SpriteIndices.clear();
    TextVertices.clear();
    TextIndices.clear();

    CurrentSceneView = nullptr;
    CurrentSpriteTexture = nullptr;
    CurrentFontResource = nullptr;
    CurrentSpritePlacementMode = ERenderPlacementMode::World;
    CurrentTextPlacementMode = ERenderPlacementMode::World;
    RHI = nullptr;
}

bool FD3D11SelectionMaskRenderer::Resize(int32 InWidth, int32 InHeight)
{
    if (RHI == nullptr || InWidth <= 0 || InHeight <= 0)
    {
        return false;
    }

    ReleaseMaskResources();
    return CreateMaskResources(InWidth, InHeight);
}

void FD3D11SelectionMaskRenderer::BeginFrame(const FSceneView* InSceneView, bool bInEnableOcclusion)
{
    CurrentSceneView = InSceneView;
    bEnableOcclusion = bInEnableOcclusion;

    PendingStaticMeshes.clear();
    PendingSprites.clear();
    PendingTexts.clear();

    SpriteVertices.clear();
    SpriteIndices.clear();
    TextVertices.clear();
    TextIndices.clear();

    CurrentSpriteTexture = nullptr;
    CurrentFontResource = nullptr;
    CurrentSpritePlacementMode = ERenderPlacementMode::World;
    CurrentTextPlacementMode = ERenderPlacementMode::World;

    if (RHI == nullptr || MaskRTV == nullptr)
    {
        return;
    }

    ActiveDepthTarget = bEnableOcclusion ? RHI->GetDepthStencilView() : MaskDSV;

    ID3D11RenderTargetView* RenderTarget = MaskRTV.Get();
    RHI->SetRenderTargets(1, &RenderTarget, ActiveDepthTarget.Get());

    if (CurrentSceneView != nullptr)
    {
        const FViewportRect& ViewRect = CurrentSceneView->GetViewRect();
        D3D11_VIEWPORT Viewport = {};
        Viewport.TopLeftX = static_cast<float>(ViewRect.X);
        Viewport.TopLeftY = static_cast<float>(ViewRect.Y);
        Viewport.Width = static_cast<float>(ViewRect.Width);
        Viewport.Height = static_cast<float>(ViewRect.Height);
        Viewport.MinDepth = 0.0f;
        Viewport.MaxDepth = 1.0f;
        RHI->SetViewport(Viewport);
    }

    static const FLOAT ClearColor[4] = {0.f, 0.f, 0.f, 0.f};
    RHI->ClearRenderTarget(MaskRTV.Get(), ClearColor);

    if (!bEnableOcclusion && MaskDSV != nullptr)
    {
        RHI->ClearDepthStencil(MaskDSV.Get(), 1.0f, 0);
    }
}

void FD3D11SelectionMaskRenderer::AddStaticMesh(const FStaticMeshRenderItem& InItem)
{
    if (!InItem.State.IsVisible() || !InItem.State.IsSelected() || InItem.RenderResource == nullptr)
    {
        return;
    }

    PendingStaticMeshes.push_back(InItem);
}

void FD3D11SelectionMaskRenderer::AddStaticMeshes(const TArray<FStaticMeshRenderItem>& InItems)
{
    size_t Index = 0;
    for (Index = 0; Index < InItems.size(); ++Index)
    {
        AddStaticMesh(InItems[Index]);
    }
}

void FD3D11SelectionMaskRenderer::AddSprite(const FSpriteRenderItem& InItem)
{
    if (!InItem.State.IsVisible() || !InItem.State.IsSelected())
    {
        return;
    }

    PendingSprites.push_back(InItem);
}

void FD3D11SelectionMaskRenderer::AddSprites(const TArray<FSpriteRenderItem>& InItems)
{
    size_t Index = 0;
    for (Index = 0; Index < InItems.size(); ++Index)
    {
        AddSprite(InItems[Index]);
    }
}

void FD3D11SelectionMaskRenderer::AddText(const FTextRenderItem& InItem)
{
    if (!InItem.State.IsVisible() || !InItem.State.IsSelected() || InItem.Text.empty() ||
        InItem.bExcludeFromOutline)
    {
        return;
    }

    PendingTexts.push_back(InItem);
}

void FD3D11SelectionMaskRenderer::AddTexts(const TArray<FTextRenderItem>& InItems)
{
    size_t Index = 0;
    for (Index = 0; Index < InItems.size(); ++Index)
    {
        AddText(InItems[Index]);
    }
}

void FD3D11SelectionMaskRenderer::EndFrame()
{
    if (RHI == nullptr || CurrentSceneView == nullptr || MaskRTV == nullptr ||
        ActiveDepthTarget == nullptr)
    {
        PendingStaticMeshes.clear();
        PendingSprites.clear();
        PendingTexts.clear();
        CurrentSceneView = nullptr;
        ActiveDepthTarget.Reset();
        return;
    }

    ID3D11RenderTargetView* RenderTarget = MaskRTV.Get();
    RHI->SetRenderTargets(1, &RenderTarget, ActiveDepthTarget.Get());

    DrawStaticMeshes();
    DrawSprites();
    DrawTexts();

    RHI->SetDefaultRenderTargets();

    PendingStaticMeshes.clear();
    PendingSprites.clear();
    PendingTexts.clear();
    CurrentSceneView = nullptr;
    ActiveDepthTarget.Reset();
}

bool FD3D11SelectionMaskRenderer::CreateMaskResources(int32 InWidth, int32 InHeight)
{
    if (RHI == nullptr || RHI->GetDevice() == nullptr || InWidth <= 0 || InHeight <= 0)
    {
        return false;
    }

    ID3D11Device* Device = RHI->GetDevice();

    D3D11_TEXTURE2D_DESC ColorDesc = {};
    ColorDesc.Width = static_cast<UINT>(InWidth);
    ColorDesc.Height = static_cast<UINT>(InHeight);
    ColorDesc.MipLevels = 1;
    ColorDesc.ArraySize = 1;
    ColorDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    ColorDesc.SampleDesc.Count = 1;
    ColorDesc.Usage = D3D11_USAGE_DEFAULT;
    ColorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(Device->CreateTexture2D(&ColorDesc, nullptr, MaskTexture.GetAddressOf())))
    {
        return false;
    }

    if (FAILED(Device->CreateRenderTargetView(MaskTexture.Get(), nullptr, MaskRTV.GetAddressOf())))
    {
        ReleaseMaskResources();
        return false;
    }

    if (FAILED(Device->CreateShaderResourceView(MaskTexture.Get(), nullptr,
                                                MaskSRV.GetAddressOf())))
    {
        ReleaseMaskResources();
        return false;
    }

    D3D11_TEXTURE2D_DESC DepthDesc = {};
    DepthDesc.Width = static_cast<UINT>(InWidth);
    DepthDesc.Height = static_cast<UINT>(InHeight);
    DepthDesc.MipLevels = 1;
    DepthDesc.ArraySize = 1;
    DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DepthDesc.SampleDesc.Count = 1;
    DepthDesc.Usage = D3D11_USAGE_DEFAULT;
    DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    if (FAILED(Device->CreateTexture2D(&DepthDesc, nullptr, MaskDepthTexture.GetAddressOf())))
    {
        ReleaseMaskResources();
        return false;
    }

    if (FAILED(Device->CreateDepthStencilView(MaskDepthTexture.Get(), nullptr,
                                              MaskDSV.GetAddressOf())))
    {
        ReleaseMaskResources();
        return false;
    }

    MaskRenderTargetBytes = GMemoryTracker.EstimateTexture2DSizeBytes(ColorDesc);
    MaskDepthBytes = GMemoryTracker.EstimateTexture2DSizeBytes(DepthDesc);
    GMemoryTracker.AddRenderTargetBytes(MaskRenderTargetBytes);
    GMemoryTracker.AddDepthStencilBytes(MaskDepthBytes);

    return true;
}

void FD3D11SelectionMaskRenderer::ReleaseMaskResources()
{
    if (MaskRenderTargetBytes > 0)
    {
        GMemoryTracker.RemoveRenderTargetBytes(MaskRenderTargetBytes);
        MaskRenderTargetBytes = 0;
    }

    if (MaskDepthBytes > 0)
    {
        GMemoryTracker.RemoveDepthStencilBytes(MaskDepthBytes);
        MaskDepthBytes = 0;
    }

    MaskDSV.Reset();
    MaskDepthTexture.Reset();
    MaskSRV.Reset();
    MaskRTV.Reset();
    MaskTexture.Reset();
}

bool FD3D11SelectionMaskRenderer::CreateStaticMeshShaders()
{
    static const D3D11_INPUT_ELEMENT_DESC LayoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    if (!RHI->CreateVertexShaderAndInputLayout(
            StaticMeshShaderPath, "VSMain", LayoutDesc, static_cast<uint32>(ARRAYSIZE(LayoutDesc)),
            StaticMeshVertexShader.GetAddressOf(), StaticMeshInputLayout.GetAddressOf()))
    {
        return false;
    }

    if (!RHI->CreatePixelShader(StaticMeshShaderPath, "PSMain",
                                StaticMeshPixelShader.GetAddressOf()))
    {
        StaticMeshInputLayout.Reset();
        StaticMeshVertexShader.Reset();
        return false;
    }

    return true;
}

bool FD3D11SelectionMaskRenderer::CreateSpriteShaders()
{
    static const D3D11_INPUT_ELEMENT_DESC LayoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    if (!RHI->CreateVertexShaderAndInputLayout(
            SpriteShaderPath, "VSMain", LayoutDesc, static_cast<uint32>(ARRAYSIZE(LayoutDesc)),
            SpriteVertexShader.GetAddressOf(), SpriteInputLayout.GetAddressOf()))
    {
        return false;
    }

    if (!RHI->CreatePixelShader(SpriteShaderPath, "PSMain", SpritePixelShader.GetAddressOf()))
    {
        SpriteInputLayout.Reset();
        SpriteVertexShader.Reset();
        return false;
    }

    return true;
}

bool FD3D11SelectionMaskRenderer::CreateTextShaders()
{
    static const D3D11_INPUT_ELEMENT_DESC LayoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    if (!RHI->CreateVertexShaderAndInputLayout(
            TextShaderPath, "VSMain", LayoutDesc, static_cast<uint32>(ARRAYSIZE(LayoutDesc)),
            TextVertexShader.GetAddressOf(), TextInputLayout.GetAddressOf()))
    {
        return false;
    }

    if (!RHI->CreatePixelShader(TextShaderPath, "PSMain", TextPixelShader.GetAddressOf()))
    {
        TextInputLayout.Reset();
        TextVertexShader.Reset();
        return false;
    }

    return true;
}

bool FD3D11SelectionMaskRenderer::CreateConstantBuffers()
{
    if (!RHI->CreateConstantBuffer(sizeof(FMeshUnlitConstants), MeshConstantBuffer.GetAddressOf()))
    {
        return false;
    }

    if (!RHI->CreateConstantBuffer(sizeof(FSpriteConstants), SpriteConstantBuffer.GetAddressOf()))
    {
        return false;
    }

    if (!RHI->CreateConstantBuffer(sizeof(FFontConstants), TextConstantBuffer.GetAddressOf()))
    {
        return false;
    }

    return true;
}

bool FD3D11SelectionMaskRenderer::CreateStates()
{
    if (RHI == nullptr || RHI->GetDevice() == nullptr)
    {
        return false;
    }

    ID3D11Device* Device = RHI->GetDevice();

    D3D11_SAMPLER_DESC SamplerDesc = {};
    SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SamplerDesc.MinLOD = 0;
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
    DepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    DepthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

    if (!RHI->CreateDepthStencilState(DepthDesc, DepthEnabledState.GetAddressOf()))
    {
        return false;
    }

    DepthDesc.DepthEnable = FALSE;
    DepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;

    if (!RHI->CreateDepthStencilState(DepthDesc, DepthDisabledState.GetAddressOf()))
    {
        return false;
    }

    D3D11_RASTERIZER_DESC RasterizerDesc = {};
    RasterizerDesc.FillMode = D3D11_FILL_SOLID;
    RasterizerDesc.CullMode = D3D11_CULL_BACK;
    RasterizerDesc.DepthClipEnable = TRUE;

    if (!RHI->CreateRasterizerState(RasterizerDesc, RasterizerState.GetAddressOf()))
    {
        return false;
    }

    return true;
}

bool FD3D11SelectionMaskRenderer::CreateDynamicBuffers()
{
    if (!RHI->CreateVertexBuffer(nullptr, sizeof(FSpriteVertex) * MaxVertexCount,
                                 sizeof(FSpriteVertex), true,
                                 DynamicSpriteVertexBuffer.GetAddressOf()))
    {
        return false;
    }

    if (!RHI->CreateIndexBuffer(nullptr, sizeof(uint32) * MaxIndexCount, true,
                                DynamicSpriteIndexBuffer.GetAddressOf()))
    {
        return false;
    }

    if (!RHI->CreateVertexBuffer(nullptr, sizeof(FFontVertex) * MaxVertexCount,
                                 sizeof(FFontVertex), true,
                                 DynamicTextVertexBuffer.GetAddressOf()))
    {
        return false;
    }

    if (!RHI->CreateIndexBuffer(nullptr, sizeof(uint32) * MaxIndexCount, true,
                                DynamicTextIndexBuffer.GetAddressOf()))
    {
        return false;
    }

    return true;
}

bool FD3D11SelectionMaskRenderer::CreateFallbackWhiteTexture()
{
    if (RHI == nullptr || RHI->GetDevice() == nullptr)
    {
        return false;
    }

    const uint32 WhitePixel = 0xFFFFFFFFu;

    D3D11_TEXTURE2D_DESC Desc = {};
    Desc.Width = 1;
    Desc.Height = 1;
    Desc.MipLevels = 1;
    Desc.ArraySize = 1;
    Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    Desc.SampleDesc.Count = 1;
    Desc.Usage = D3D11_USAGE_DEFAULT;
    Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA InitialData = {};
    InitialData.pSysMem = &WhitePixel;
    InitialData.SysMemPitch = sizeof(uint32);

    if (FAILED(RHI->GetDevice()->CreateTexture2D(&Desc, &InitialData,
                                                 FallbackWhiteTexture.GetAddressOf())))
    {
        return false;
    }

    if (FAILED(RHI->GetDevice()->CreateShaderResourceView(FallbackWhiteTexture.Get(), nullptr,
                                                          FallbackWhiteSRV.GetAddressOf())))
    {
        FallbackWhiteTexture.Reset();
        return false;
    }

    return true;
}

void FD3D11SelectionMaskRenderer::DrawStaticMeshes()
{
    if (PendingStaticMeshes.empty() || CurrentSceneView == nullptr)
    {
        return;
    }

    RHI->SetInputLayout(StaticMeshInputLayout.Get());
    RHI->SetVertexShader(StaticMeshVertexShader.Get());
    RHI->SetVSConstantBuffer(0, MeshConstantBuffer.Get());
    RHI->SetPixelShader(StaticMeshPixelShader.Get());
    RHI->SetRasterizerState(RasterizerState.Get());
    RHI->SetDepthStencilState(bEnableOcclusion ? DepthEnabledState.Get() : DepthDisabledState.Get(),
                              0);
    RHI->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    size_t MeshIndex = 0;
    for (MeshIndex = 0; MeshIndex < PendingStaticMeshes.size(); ++MeshIndex)
    {
        const FStaticMeshRenderItem& DrawItem = PendingStaticMeshes[MeshIndex];
        const FStaticMesh* Resource = DrawItem.RenderResource;
        if (Resource == nullptr || Resource->VertexBuffer == nullptr || Resource->IndexBuffer == nullptr)
        {
            continue;
        }

        const UINT Stride = Resource->VertexStride;
        const UINT Offset = 0;
        ID3D11Buffer* VertexBuffer = Resource->VertexBuffer.Get();

        RHI->SetVertexBuffer(0, VertexBuffer, Stride, Offset);
        RHI->SetIndexBuffer(Resource->IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

        FMeshUnlitConstants Constants = {};
        Constants.MVP = DrawItem.World * CurrentSceneView->GetViewProjectionMatrix();
        Constants.BaseColor = FColor::White();

        if (!RHI->UpdateConstantBuffer(MeshConstantBuffer.Get(), &Constants, sizeof(Constants)))
        {
            continue;
        }

        size_t SubMeshIndex = 0;
        for (SubMeshIndex = 0; SubMeshIndex < Resource->SubMeshes.size(); ++SubMeshIndex)
        {
            const FSubMesh& SubMesh = Resource->SubMeshes[SubMeshIndex];
            RHI->DrawIndexed(SubMesh.IndexCount, SubMesh.StartIndexLocation, 0);
        }
    }
}

void FD3D11SelectionMaskRenderer::DrawSprites()
{
    if (PendingSprites.empty() || CurrentSceneView == nullptr)
    {
        return;
    }

    ProcessSortedSprites();
    FlushSpriteBatch();
}

void FD3D11SelectionMaskRenderer::DrawTexts()
{
    if (PendingTexts.empty() || CurrentSceneView == nullptr)
    {
        return;
    }

    ProcessSortedTexts();
    FlushTextBatch();
}

void FD3D11SelectionMaskRenderer::ProcessSortedSprites()
{
    std::sort(PendingSprites.begin(), PendingSprites.end(), FSpriteRenderItemLess{CurrentSceneView});

    size_t Index = 0;
    for (Index = 0; Index < PendingSprites.size(); ++Index)
    {
        const FSpriteRenderItem& Item = PendingSprites[Index];
        const FSpriteBatchKey BatchKey = MakeSpriteBatchKey(Item);

        if (Index == 0 || !(BatchKey == MakeSpriteBatchKey(PendingSprites[Index - 1])))
        {
            FlushSpriteBatch();
            BeginSpriteBatch(BatchKey);
        }

        AppendSpriteItem(Item);
    }
}

void FD3D11SelectionMaskRenderer::ProcessSortedTexts()
{
    std::sort(PendingTexts.begin(), PendingTexts.end(), FTextRenderItemLess{CurrentSceneView});

    size_t Index = 0;
    for (Index = 0; Index < PendingTexts.size(); ++Index)
    {
        const FTextRenderItem& Item = PendingTexts[Index];
        const FTextBatchKey BatchKey = MakeTextBatchKey(Item);

        if (Index == 0 || !(BatchKey == MakeTextBatchKey(PendingTexts[Index - 1])))
        {
            FlushTextBatch();
            BeginTextBatch(BatchKey);
        }

        AppendTextItem(Item);
    }
}

FD3D11SelectionMaskRenderer::FSpriteBatchKey
FD3D11SelectionMaskRenderer::MakeSpriteBatchKey(const FSpriteRenderItem& InItem) const
{
    FSpriteBatchKey Key = {};
    Key.TextureResource = InItem.TextureResource;
    Key.PlacementMode = InItem.Placement.Mode;
    return Key;
}

void FD3D11SelectionMaskRenderer::BeginSpriteBatch(const FSpriteBatchKey& InBatchKey)
{
    CurrentSpriteTexture = InBatchKey.TextureResource;
    CurrentSpritePlacementMode = InBatchKey.PlacementMode;
}

void FD3D11SelectionMaskRenderer::AppendSpriteItem(const FSpriteRenderItem& InItem)
{
    if (!CanAppendSpriteQuad())
    {
        FlushSpriteBatch();
        BeginSpriteBatch(MakeSpriteBatchKey(InItem));
    }

    const FMatrix& PlacementWorld = InItem.Placement.World;
    FVector SpriteOrigin = PlacementWorld.GetOrigin() + InItem.Placement.WorldOffset;

    FVector RightAxis;
    FVector UpAxis;

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
        const FVector WorldScale = PlacementWorld.GetScaleVector();
        RightAxis = PlacementWorld.GetForwardVector().GetSafeNormal() * WorldScale.X;
        UpAxis = PlacementWorld.GetUpVector().GetSafeNormal() * WorldScale.Z;
    }

    const FVector BottomLeft = SpriteOrigin - RightAxis - UpAxis;
    AppendSpriteQuad(BottomLeft, RightAxis * 2.0f, UpAxis * 2.0f, InItem.UVMin, InItem.UVMax);
}

void FD3D11SelectionMaskRenderer::AppendSpriteQuad(const FVector& InBottomLeft,
                                                   const FVector& InRight, const FVector& InUp,
                                                   const FVector2& InUVMin, const FVector2& InUVMax)
{
    const uint32 BaseVertex = static_cast<uint32>(SpriteVertices.size());

    FSpriteVertex V0 = {};
    V0.Position = InBottomLeft;
    V0.UV = FVector2(InUVMin.X, InUVMax.Y);
    V0.Color = FColor::White();

    FSpriteVertex V1 = {};
    V1.Position = InBottomLeft + InUp;
    V1.UV = FVector2(InUVMin.X, InUVMin.Y);
    V1.Color = FColor::White();

    FSpriteVertex V2 = {};
    V2.Position = InBottomLeft + InRight + InUp;
    V2.UV = FVector2(InUVMax.X, InUVMin.Y);
    V2.Color = FColor::White();

    FSpriteVertex V3 = {};
    V3.Position = InBottomLeft + InRight;
    V3.UV = FVector2(InUVMax.X, InUVMax.Y);
    V3.Color = FColor::White();

    SpriteVertices.push_back(V0);
    SpriteVertices.push_back(V1);
    SpriteVertices.push_back(V2);
    SpriteVertices.push_back(V3);

    SpriteIndices.push_back(BaseVertex + 0);
    SpriteIndices.push_back(BaseVertex + 1);
    SpriteIndices.push_back(BaseVertex + 2);
    SpriteIndices.push_back(BaseVertex + 0);
    SpriteIndices.push_back(BaseVertex + 2);
    SpriteIndices.push_back(BaseVertex + 3);
}

FD3D11SelectionMaskRenderer::FTextBatchKey
FD3D11SelectionMaskRenderer::MakeTextBatchKey(const FTextRenderItem& InItem) const
{
    FTextBatchKey Key = {};
    Key.FontResource = InItem.FontResource;
    Key.PlacementMode = InItem.Placement.Mode;
    return Key;
}

void FD3D11SelectionMaskRenderer::BeginTextBatch(const FTextBatchKey& InBatchKey)
{
    CurrentFontResource = InBatchKey.FontResource;
    CurrentTextPlacementMode = InBatchKey.PlacementMode;
}

void FD3D11SelectionMaskRenderer::AppendTextItem(const FTextRenderItem& InItem)
{
    if (InItem.FontResource == nullptr)
    {
        AppendTextNullFontFallback(InItem);
        return;
    }

    if (InItem.LayoutMode == ETextLayoutMode::FitToBox)
    {
        AppendTextItemFitToBox(InItem);
    }
    else
    {
        AppendTextItemNatural(InItem);
    }
}

void FD3D11SelectionMaskRenderer::AppendTextItemNatural(const FTextRenderItem& InItem)
{
    const FTextLayout Layout = BuildTextLayout(InItem);
    if (!Layout.IsValid())
    {
        return;
    }

    FVector TextOrigin;
    FVector RightAxis;
    FVector UpAxis;
    BuildPlacementAxes(InItem.Placement, TextOrigin, RightAxis, UpAxis);

    const float CenterX = (Layout.MinX + Layout.MaxX) * 0.5f;
    const float CenterY = (Layout.MinY + Layout.MaxY) * 0.5f;

    size_t GlyphIndex = 0;
    for (GlyphIndex = 0; GlyphIndex < Layout.Glyphs.size(); ++GlyphIndex)
    {
        const FLaidOutGlyph& Glyph = Layout.Glyphs[GlyphIndex];
        const float X0 = Glyph.MinX - CenterX;
        const float YBottom = Glyph.MaxY - CenterY;

        const FVector BottomLeft = TextOrigin + RightAxis * X0 - UpAxis * YBottom;
        const FVector GlyphRight = RightAxis * (Glyph.MaxX - Glyph.MinX);
        const FVector GlyphUp = UpAxis * (Glyph.MaxY - Glyph.MinY);

        if (Glyph.bSolidColorQuad || Glyph.Glyph == nullptr)
        {
            AppendSolidQuad(BottomLeft, GlyphRight, GlyphUp);
        }
        else
        {
            AppendGlyphQuad(BottomLeft, GlyphRight, GlyphUp, *Glyph.Glyph, *InItem.FontResource);
        }
    }
}

void FD3D11SelectionMaskRenderer::AppendTextItemFitToBox(const FTextRenderItem& InItem)
{
    const FTextLayout Layout = BuildTextLayout(InItem);
    if (!Layout.IsValid())
    {
        return;
    }

    FVector TextOrigin;
    FVector RightAxis;
    FVector UpAxis;
    BuildPlacementAxes(InItem.Placement, TextOrigin, RightAxis, UpAxis);

    const FVector WorldScale = InItem.Placement.World.GetScaleVector();
    const float BoxWidth = std::max(WorldScale.X, 1.0f);
    const float BoxHeight = std::max(WorldScale.Y, 1.0f);
    const float UniformScale = std::min(BoxWidth / Layout.GetWidth(), BoxHeight / Layout.GetHeight());

    const float FinalWidth = Layout.GetWidth() * UniformScale;
    const float FinalHeight = Layout.GetHeight() * UniformScale;
    const float OffsetX = (BoxWidth - FinalWidth) * 0.5f;
    const float OffsetY = (BoxHeight - FinalHeight) * 0.5f;

    size_t GlyphIndex = 0;
    for (GlyphIndex = 0; GlyphIndex < Layout.Glyphs.size(); ++GlyphIndex)
    {
        const FLaidOutGlyph& Glyph = Layout.Glyphs[GlyphIndex];

        const float X0 = OffsetX + (Glyph.MinX - Layout.MinX) * UniformScale;
        const float Y0 = OffsetY + (Glyph.MinY - Layout.MinY) * UniformScale;
        const float W = (Glyph.MaxX - Glyph.MinX) * UniformScale;
        const float H = (Glyph.MaxY - Glyph.MinY) * UniformScale;

        const FVector BottomLeft = TextOrigin + RightAxis * X0 - UpAxis * Y0;
        const FVector GlyphRight = RightAxis * W;
        const FVector GlyphUp = -UpAxis * H;

        if (Glyph.bSolidColorQuad || Glyph.Glyph == nullptr)
        {
            AppendSolidQuad(BottomLeft, GlyphRight, GlyphUp);
        }
        else
        {
            AppendGlyphQuad(BottomLeft, GlyphRight, GlyphUp, *Glyph.Glyph, *InItem.FontResource);
        }
    }
}

void FD3D11SelectionMaskRenderer::AppendTextNullFontFallback(const FTextRenderItem& InItem)
{
    FVector Origin;
    FVector RightAxis;
    FVector UpAxis;
    BuildPlacementAxes(InItem.Placement, Origin, RightAxis, UpAxis);

    float Width = 1.0f;
    float Height = 1.0f;

    if (InItem.LayoutMode == ETextLayoutMode::FitToBox)
    {
        const FVector WorldScale = InItem.Placement.World.GetScaleVector();
        Width = std::max(WorldScale.X, 1.0f);
        Height = std::max(WorldScale.Y, 1.0f);
    }
    else
    {
        const float FallbackExtent = std::max(InItem.TextScale, 1.0f);
        Width = FallbackExtent;
        Height = FallbackExtent;
    }

    const FVector QuadRight = RightAxis * Width;
    const FVector QuadUp = UpAxis * Height;
    const FVector BottomLeft = Origin - QuadRight * 0.5f - QuadUp * 0.5f;

    AppendSolidQuad(BottomLeft, QuadRight, QuadUp);
}

void FD3D11SelectionMaskRenderer::AppendGlyphQuad(const FVector& InBottomLeft,
                                                  const FVector& InRight, const FVector& InUp,
                                                  const FFontGlyph& InGlyph,
                                                  const FFontResource& InFont)
{
    if (!CanAppendTextQuad())
    {
        FlushTextBatch();
    }

    const uint32 BaseVertex = static_cast<uint32>(TextVertices.size());

    const float InvAtlasWidth = InFont.GetInvAtlasWidth();
    const float InvAtlasHeight = InFont.GetInvAtlasHeight();

    const float U0 = static_cast<float>(InGlyph.X) * InvAtlasWidth;
    const float V0 = static_cast<float>(InGlyph.Y) * InvAtlasHeight;
    const float U1 = static_cast<float>(InGlyph.X + InGlyph.Width) * InvAtlasWidth;
    const float V1 = static_cast<float>(InGlyph.Y + InGlyph.Height) * InvAtlasHeight;

    FFontVertex Vtx0 = {};
    Vtx0.Position = InBottomLeft;
    Vtx0.UV = FVector2(U0, V1);
    Vtx0.Color = FColor::White();

    FFontVertex Vtx1 = {};
    Vtx1.Position = InBottomLeft + InUp;
    Vtx1.UV = FVector2(U0, V0);
    Vtx1.Color = FColor::White();

    FFontVertex Vtx2 = {};
    Vtx2.Position = InBottomLeft + InRight + InUp;
    Vtx2.UV = FVector2(U1, V0);
    Vtx2.Color = FColor::White();

    FFontVertex Vtx3 = {};
    Vtx3.Position = InBottomLeft + InRight;
    Vtx3.UV = FVector2(U1, V1);
    Vtx3.Color = FColor::White();

    TextVertices.push_back(Vtx0);
    TextVertices.push_back(Vtx1);
    TextVertices.push_back(Vtx2);
    TextVertices.push_back(Vtx3);

    TextIndices.push_back(BaseVertex + 0);
    TextIndices.push_back(BaseVertex + 1);
    TextIndices.push_back(BaseVertex + 2);
    TextIndices.push_back(BaseVertex + 0);
    TextIndices.push_back(BaseVertex + 2);
    TextIndices.push_back(BaseVertex + 3);
}

void FD3D11SelectionMaskRenderer::AppendSolidQuad(const FVector& InBottomLeft,
                                                  const FVector& InRight, const FVector& InUp)
{
    if (!CanAppendTextQuad())
    {
        FlushTextBatch();
    }

    const uint32 BaseVertex = static_cast<uint32>(TextVertices.size());

    FFontVertex Vtx0 = {};
    Vtx0.Position = InBottomLeft;
    Vtx0.UV = FVector2(0.0f, 1.0f);
    Vtx0.Color = FColor::White();

    FFontVertex Vtx1 = {};
    Vtx1.Position = InBottomLeft + InUp;
    Vtx1.UV = FVector2(0.0f, 0.0f);
    Vtx1.Color = FColor::White();

    FFontVertex Vtx2 = {};
    Vtx2.Position = InBottomLeft + InRight + InUp;
    Vtx2.UV = FVector2(1.0f, 0.0f);
    Vtx2.Color = FColor::White();

    FFontVertex Vtx3 = {};
    Vtx3.Position = InBottomLeft + InRight;
    Vtx3.UV = FVector2(1.0f, 1.0f);
    Vtx3.Color = FColor::White();

    TextVertices.push_back(Vtx0);
    TextVertices.push_back(Vtx1);
    TextVertices.push_back(Vtx2);
    TextVertices.push_back(Vtx3);

    TextIndices.push_back(BaseVertex + 0);
    TextIndices.push_back(BaseVertex + 1);
    TextIndices.push_back(BaseVertex + 2);
    TextIndices.push_back(BaseVertex + 0);
    TextIndices.push_back(BaseVertex + 2);
    TextIndices.push_back(BaseVertex + 3);
}

bool FD3D11SelectionMaskRenderer::CanAppendSpriteQuad() const
{
    return SpriteVertices.size() + 4 <= MaxVertexCount && SpriteIndices.size() + 6 <= MaxIndexCount;
}

bool FD3D11SelectionMaskRenderer::CanAppendTextQuad() const
{
    return TextVertices.size() + 4 <= MaxVertexCount && TextIndices.size() + 6 <= MaxIndexCount;
}

void FD3D11SelectionMaskRenderer::FlushSpriteBatch()
{
    if (SpriteVertices.empty() || SpriteIndices.empty() || CurrentSceneView == nullptr)
    {
        return;
    }

    if (!RHI->UpdateDynamicBuffer(DynamicSpriteVertexBuffer.Get(), SpriteVertices.data(),
                                  static_cast<uint32>(sizeof(FSpriteVertex) * SpriteVertices.size())))
    {
        SpriteVertices.clear();
        SpriteIndices.clear();
        return;
    }

    if (!RHI->UpdateDynamicBuffer(DynamicSpriteIndexBuffer.Get(), SpriteIndices.data(),
                                  static_cast<uint32>(sizeof(uint32) * SpriteIndices.size())))
    {
        SpriteVertices.clear();
        SpriteIndices.clear();
        return;
    }

    FSpriteConstants Constants = {};
    Constants.VP = CurrentSceneView->GetViewProjectionMatrix();

    if (!RHI->UpdateConstantBuffer(SpriteConstantBuffer.Get(), &Constants, sizeof(Constants)))
    {
        SpriteVertices.clear();
        SpriteIndices.clear();
        return;
    }

    const UINT VertexStride = sizeof(FSpriteVertex);
    const UINT VertexOffset = 0;
    ID3D11Buffer* VertexBuffer = DynamicSpriteVertexBuffer.Get();

    RHI->SetInputLayout(SpriteInputLayout.Get());
    RHI->SetVertexShader(SpriteVertexShader.Get());
    RHI->SetVSConstantBuffer(0, SpriteConstantBuffer.Get());
    RHI->SetPixelShader(SpritePixelShader.Get());
    RHI->SetRasterizerState(RasterizerState.Get());
    RHI->SetDepthStencilState(bEnableOcclusion ? DepthEnabledState.Get() : DepthDisabledState.Get(),
                              0);

    const float BlendFactor[4] = {0.f, 0.f, 0.f, 0.f};
    RHI->SetBlendState(AlphaBlendState.Get(), BlendFactor);
    RHI->SetPSSampler(0, SamplerState.Get());
    RHI->SetPSShaderResource(0, ResolveSpriteSRV(CurrentSpriteTexture));

    RHI->SetVertexBuffer(0, VertexBuffer, VertexStride, VertexOffset);
    RHI->SetIndexBuffer(DynamicSpriteIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    RHI->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    RHI->DrawIndexed(static_cast<uint32>(SpriteIndices.size()), 0, 0);

    RHI->ClearPSShaderResource(0);
    RHI->ClearBlendState();

    SpriteVertices.clear();
    SpriteIndices.clear();
}

void FD3D11SelectionMaskRenderer::FlushTextBatch()
{
    if (TextVertices.empty() || TextIndices.empty() || CurrentSceneView == nullptr)
    {
        return;
    }

    if (!RHI->UpdateDynamicBuffer(DynamicTextVertexBuffer.Get(), TextVertices.data(),
                                  static_cast<uint32>(sizeof(FFontVertex) * TextVertices.size())))
    {
        TextVertices.clear();
        TextIndices.clear();
        return;
    }

    if (!RHI->UpdateDynamicBuffer(DynamicTextIndexBuffer.Get(), TextIndices.data(),
                                  static_cast<uint32>(sizeof(uint32) * TextIndices.size())))
    {
        TextVertices.clear();
        TextIndices.clear();
        return;
    }

    FFontConstants Constants = {};
    Constants.VP = CurrentSceneView->GetViewProjectionMatrix();
    Constants.TintColor = FColor::White();

    if (!RHI->UpdateConstantBuffer(TextConstantBuffer.Get(), &Constants, sizeof(Constants)))
    {
        TextVertices.clear();
        TextIndices.clear();
        return;
    }

    const UINT VertexStride = sizeof(FFontVertex);
    const UINT VertexOffset = 0;
    ID3D11Buffer* VertexBuffer = DynamicTextVertexBuffer.Get();

    RHI->SetInputLayout(TextInputLayout.Get());
    RHI->SetVertexShader(TextVertexShader.Get());
    RHI->SetVSConstantBuffer(0, TextConstantBuffer.Get());
    RHI->SetPixelShader(TextPixelShader.Get());
    RHI->SetPSConstantBuffer(0, TextConstantBuffer.Get());
    RHI->SetRasterizerState(RasterizerState.Get());
    RHI->SetDepthStencilState(bEnableOcclusion ? DepthEnabledState.Get() : DepthDisabledState.Get(),
                              0);

    const float BlendFactor[4] = {0.f, 0.f, 0.f, 0.f};
    RHI->SetBlendState(AlphaBlendState.Get(), BlendFactor);
    RHI->SetPSSampler(0, SamplerState.Get());
    RHI->SetPSShaderResource(0, ResolveFontSRV(CurrentFontResource));

    RHI->SetVertexBuffer(0, VertexBuffer, VertexStride, VertexOffset);
    RHI->SetIndexBuffer(DynamicTextIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    RHI->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    RHI->DrawIndexed(static_cast<uint32>(TextIndices.size()), 0, 0);

    RHI->ClearPSShaderResource(0);
    RHI->ClearBlendState();

    TextVertices.clear();
    TextIndices.clear();
}

ID3D11ShaderResourceView* FD3D11SelectionMaskRenderer::ResolveSpriteSRV(
    const FTextureResource* InTextureResource) const
{
    if (InTextureResource != nullptr && InTextureResource->GetSRV() != nullptr)
    {
        return InTextureResource->GetSRV();
    }

    return FallbackWhiteSRV.Get();
}

ID3D11ShaderResourceView* FD3D11SelectionMaskRenderer::ResolveFontSRV(
    const FFontResource* InFontResource) const
{
    if (InFontResource != nullptr && InFontResource->GetSRV() != nullptr)
    {
        return InFontResource->GetSRV();
    }

    return FallbackWhiteSRV.Get();
}

FD3D11SelectionMaskRenderer::FResolvedGlyph
FD3D11SelectionMaskRenderer::ResolveGlyph(const FFontResource& InFont, uint32 InCodePoint) const
{
    const FFontGlyph* Glyph = InFont.FindGlyph(InCodePoint);
    if (Glyph != nullptr && Glyph->IsValid())
    {
        FResolvedGlyph Result = {};
        Result.Glyph = Glyph;
        Result.Kind = EResolvedGlyphKind::Normal;
        return Result;
    }

    Glyph = InFont.FindGlyph(static_cast<uint32>('?'));
    if (Glyph != nullptr && Glyph->IsValid())
    {
        FResolvedGlyph Result = {};
        Result.Glyph = Glyph;
        Result.Kind = EResolvedGlyphKind::QuestionFallback;
        return Result;
    }

    FResolvedGlyph Result = {};
    Result.Glyph = nullptr;
    Result.Kind = EResolvedGlyphKind::Missing;
    return Result;
}

float FD3D11SelectionMaskRenderer::GetMissingGlyphAdvance(const FFontResource& InFont,
                                                          float InLineHeight,
                                                          float InScale) const
{
    const FFontGlyph* SpaceGlyph = InFont.FindGlyph(static_cast<uint32>(' '));
    if (SpaceGlyph != nullptr && SpaceGlyph->IsValid())
    {
        return static_cast<float>(SpaceGlyph->XAdvance) * InScale;
    }

    return InLineHeight * 0.5f * InScale;
}

FD3D11SelectionMaskRenderer::FTextLayout
FD3D11SelectionMaskRenderer::BuildTextLayout(const FTextRenderItem& InItem) const
{
    FTextLayout Layout;

    if (InItem.FontResource == nullptr || InItem.Text.empty())
    {
        return Layout;
    }

    const float RawLineHeight = (InItem.FontResource->Common.LineHeight > 0)
                                    ? static_cast<float>(InItem.FontResource->Common.LineHeight)
                                    : DefaultLineHeight;
    const float UnitScale = (RawLineHeight > 0.0f) ? (1.0f / RawLineHeight) : (1.0f / DefaultLineHeight);
    const float Scale = (InItem.TextScale > 0.0f) ? (InItem.TextScale * UnitScale) : UnitScale;
    const float LetterSpacing = InItem.LetterSpacing * UnitScale;
    const float LineSpacing = InItem.LineSpacing * UnitScale;
    const float MissingAdvance = GetMissingGlyphAdvance(*InItem.FontResource, RawLineHeight, Scale);
    const float MissingWidth = MissingAdvance;
    const float MissingHeight = RawLineHeight * Scale;

    float PenX = 0.0f;
    float PenY = 0.0f;
    bool bHasBounds = false;

    const TArray<uint32> CodePoints = DecodeUtf8CodePoints(InItem.Text);

    size_t CodePointIndex = 0;
    for (CodePointIndex = 0; CodePointIndex < CodePoints.size(); ++CodePointIndex)
    {
        const uint32 CodePoint = CodePoints[CodePointIndex];

        if (CodePoint == static_cast<uint32>('\r'))
        {
            continue;
        }

        if (CodePoint == static_cast<uint32>('\n'))
        {
            PenX = 0.0f;
            PenY += RawLineHeight * Scale + LineSpacing;
            continue;
        }

        if (CodePoint == static_cast<uint32>(' '))
        {
            float SpaceAdvance = 0.0f;
            const FFontGlyph* SpaceGlyph = InItem.FontResource->FindGlyph(static_cast<uint32>(' '));
            if (SpaceGlyph != nullptr && SpaceGlyph->IsValid())
            {
                SpaceAdvance = static_cast<float>(SpaceGlyph->XAdvance) * Scale;
            }

            if (SpaceAdvance <= 0.0f)
            {
                SpaceAdvance = RawLineHeight * 0.25f * Scale;
            }

            PenX += SpaceAdvance;
            continue;
        }

        const FResolvedGlyph Resolved = ResolveGlyph(*InItem.FontResource, CodePoint);

        FLaidOutGlyph OutGlyph = {};

        if (Resolved.Kind == EResolvedGlyphKind::Missing || Resolved.Glyph == nullptr)
        {
            OutGlyph.Glyph = nullptr;
            OutGlyph.bSolidColorQuad = true;
            OutGlyph.MinX = PenX;
            OutGlyph.MinY = PenY;
            OutGlyph.MaxX = PenX + MissingWidth;
            OutGlyph.MaxY = PenY + MissingHeight;

            PenX += MissingAdvance + LetterSpacing;
        }
        else
        {
            OutGlyph.Glyph = Resolved.Glyph;
            OutGlyph.bSolidColorQuad = false;
            OutGlyph.MinX = PenX + static_cast<float>(Resolved.Glyph->XOffset) * Scale;
            OutGlyph.MinY = PenY + static_cast<float>(Resolved.Glyph->YOffset) * Scale;
            OutGlyph.MaxX = OutGlyph.MinX + static_cast<float>(Resolved.Glyph->Width) * Scale;
            OutGlyph.MaxY = OutGlyph.MinY + static_cast<float>(Resolved.Glyph->Height) * Scale;

            PenX += Scale * static_cast<float>(Resolved.Glyph->XAdvance);
            PenX += LetterSpacing;
        }

        Layout.Glyphs.push_back(OutGlyph);

        if (!bHasBounds)
        {
            Layout.MinX = OutGlyph.MinX;
            Layout.MinY = OutGlyph.MinY;
            Layout.MaxX = OutGlyph.MaxX;
            Layout.MaxY = OutGlyph.MaxY;
            bHasBounds = true;
        }
        else
        {
            Layout.MinX = std::min(Layout.MinX, OutGlyph.MinX);
            Layout.MinY = std::min(Layout.MinY, OutGlyph.MinY);
            Layout.MaxX = std::max(Layout.MaxX, OutGlyph.MaxX);
            Layout.MaxY = std::max(Layout.MaxY, OutGlyph.MaxY);
        }
    }

    return Layout;
}

void FD3D11SelectionMaskRenderer::BuildPlacementAxes(const FRenderPlacement& InPlacement,
                                                     FVector& OutOrigin,
                                                     FVector& OutRightAxis,
                                                     FVector& OutUpAxis) const
{
    const FMatrix& PlacementWorld = InPlacement.World;
    OutOrigin = PlacementWorld.GetOrigin() + InPlacement.WorldOffset;

    if (InPlacement.IsBillboard())
    {
        const FMatrix CameraWorld = CurrentSceneView->GetViewMatrix().GetInverse();
        OutRightAxis = CameraWorld.GetRightVector().GetSafeNormal();
        OutUpAxis = CameraWorld.GetUpVector().GetSafeNormal();
    }
    else
    {
        OutRightAxis = PlacementWorld.GetRightVector().GetSafeNormal();
        OutUpAxis = PlacementWorld.GetUpVector().GetSafeNormal();
    }
}
