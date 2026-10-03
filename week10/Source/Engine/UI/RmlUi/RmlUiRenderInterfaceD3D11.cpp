#include "UI/RmlUi/RmlUiRenderInterfaceD3D11.h"

#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Render/Resource/Texture.h"

#include <algorithm>
#include <cstddef>
#include <d3dcompiler.h>
#include <filesystem>
#include <vector>

namespace
{
    constexpr const char* RmlUiShaderSource = R"(
cbuffer RmlUiFrameConstants : register(b0)
{
    float2 ViewportSize;
    float2 Translation;
    float2 RenderScale;
    uint UseTexture;
    float Padding;
    float4x4 Transform;
};

Texture2D RmlUiTexture : register(t0);
SamplerState RmlUiSampler : register(s0);

struct VSInput
{
    float2 Position : POSITION;
    float4 Color : COLOR0;
    float2 TexCoord : TEXCOORD0;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR0;
    float2 TexCoord : TEXCOORD0;
};

VSOutput VSMain(VSInput Input)
{
    VSOutput Output;
    float4 LocalPosition = float4(Input.Position + Translation, 0.0f, 1.0f);
    float4 TransformedPosition = mul(Transform, LocalPosition);
    float2 PixelPosition = TransformedPosition.xy * RenderScale;
    float2 ClipPosition;
    ClipPosition.x = (PixelPosition.x / max(ViewportSize.x, 1.0f)) * 2.0f - 1.0f;
    ClipPosition.y = 1.0f - (PixelPosition.y / max(ViewportSize.y, 1.0f)) * 2.0f;
    Output.Position = float4(ClipPosition, 0.0f, 1.0f);
    Output.Color = Input.Color;
    Output.TexCoord = Input.TexCoord;
    return Output;
}

float4 PSMain(VSOutput Input) : SV_TARGET
{
    float4 Color = Input.Color;
    if (UseTexture != 0)
    {
        float4 TextureColor = RmlUiTexture.Sample(RmlUiSampler, Input.TexCoord);
        TextureColor.rgb *= TextureColor.a;
        Color *= TextureColor;
    }
    return Color;
}
)";

    bool CompileRmlUiShader(const char* EntryPoint, const char* Target, TComPtr<ID3DBlob>& OutBlob)
    {
        UINT Flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
        Flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        TComPtr<ID3DBlob> ErrorBlob;
        const HRESULT Hr = D3DCompile(
            RmlUiShaderSource,
            strlen(RmlUiShaderSource),
            "RmlUiRuntimeShader",
            nullptr,
            nullptr,
            EntryPoint,
            Target,
            Flags,
            0,
            OutBlob.ReleaseAndGetAddressOf(),
            ErrorBlob.ReleaseAndGetAddressOf());

        if (FAILED(Hr))
        {
            const char* Message = ErrorBlob ? static_cast<const char*>(ErrorBlob->GetBufferPointer()) : "Unknown shader compile error.";
            UE_LOG_ERROR("[RmlUi] Failed to compile %s: %s", EntryPoint, Message);
            return false;
        }

        return true;
    }

    FString NormalizeRmlUiTexturePath(const Rml::String& Source)
    {
        FString Path = FPaths::Normalize(Source);
        if (Path.empty())
        {
            return Source;
        }

        const auto ExistsRelativeToRoot = [](const FString& Candidate)
        {
            return std::filesystem::exists(std::filesystem::path(FPaths::ToAbsolute(FPaths::ToWide(Candidate))));
        };

        if (ExistsRelativeToRoot(Path))
        {
            return Path;
        }

        const FString AssetPrefix = "Asset/";
        size_t AssetPos = Path.rfind(AssetPrefix);
        if (AssetPos != FString::npos)
        {
            FString AssetRelativePath = Path.substr(AssetPos);
            if (ExistsRelativeToRoot(AssetRelativePath))
            {
                return AssetRelativePath;
            }
        }

        return Path;
    }
}

bool FRmlUiRenderInterfaceD3D11::Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InContext)
{
    if (!InDevice || !InContext)
    {
        UE_LOG_ERROR("[RmlUi] Cannot initialize D3D11 render interface without device/context.");
        return false;
    }

    Device = InDevice;
    Context = InContext;

    if (!CreateShaders() || !CreateStates())
    {
        Shutdown();
        return false;
    }

    UE_LOG("[RmlUi] D3D11 render interface initialized.");
    return true;
}

void FRmlUiRenderInterfaceD3D11::Shutdown()
{
    GeometryByHandle.clear();
    GeneratedTextures.clear();

    ScissorRasterizerState.Reset();
    RasterizerState.Reset();
    DepthStencilState.Reset();
    BlendState.Reset();
    SamplerState.Reset();
    ConstantBuffer.Reset();
    InputLayout.Reset();
    PixelShader.Reset();
    VertexShader.Reset();
    Context.Reset();
    Device.Reset();
}

void FRmlUiRenderInterfaceD3D11::BeginFrame(
    const Rml::Vector2f& InViewportMin,
    const Rml::Vector2f& InViewportSize,
    const Rml::Vector2f& InRenderScale)
{
    ViewportMin = InViewportMin;
    ViewportSize = Rml::Vector2f(std::max(InViewportSize.x, 1.0f), std::max(InViewportSize.y, 1.0f));
    RenderScale = Rml::Vector2f(std::max(InRenderScale.x, 0.001f), std::max(InRenderScale.y, 0.001f));
    CurrentTransform = Rml::Matrix4f::Identity();
    bScissorEnabled = false;
    CurrentScissorRegion = Rml::Rectanglei::MakeInvalid();

    if (!Context)
    {
        return;
    }

    D3D11_VIEWPORT Viewport = {};
    Viewport.TopLeftX = ViewportMin.x;
    Viewport.TopLeftY = ViewportMin.y;
    Viewport.Width = ViewportSize.x;
    Viewport.Height = ViewportSize.y;
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;
    Context->RSSetViewports(1, &Viewport);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ApplyScissorState();
}

Rml::CompiledGeometryHandle FRmlUiRenderInterfaceD3D11::CompileGeometry(Rml::Span<const Rml::Vertex> Vertices, Rml::Span<const int> Indices)
{
    if (!Device || Vertices.empty() || Indices.empty())
    {
        return 0;
    }

    FCompiledGeometry Geometry;

    D3D11_BUFFER_DESC VertexDesc = {};
    VertexDesc.ByteWidth = static_cast<UINT>(sizeof(Rml::Vertex) * Vertices.size());
    VertexDesc.Usage = D3D11_USAGE_IMMUTABLE;
    VertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA VertexData = {};
    VertexData.pSysMem = Vertices.data();
    if (FAILED(Device->CreateBuffer(&VertexDesc, &VertexData, Geometry.VertexBuffer.ReleaseAndGetAddressOf())))
    {
        UE_LOG_ERROR("[RmlUi] Failed to create geometry vertex buffer.");
        return 0;
    }

    std::vector<uint32> D3DIndices;
    D3DIndices.reserve(Indices.size());
    for (int Index : Indices)
    {
        D3DIndices.push_back(static_cast<uint32>(Index));
    }

    D3D11_BUFFER_DESC IndexDesc = {};
    IndexDesc.ByteWidth = static_cast<UINT>(sizeof(uint32) * D3DIndices.size());
    IndexDesc.Usage = D3D11_USAGE_IMMUTABLE;
    IndexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA IndexData = {};
    IndexData.pSysMem = D3DIndices.data();
    if (FAILED(Device->CreateBuffer(&IndexDesc, &IndexData, Geometry.IndexBuffer.ReleaseAndGetAddressOf())))
    {
        UE_LOG_ERROR("[RmlUi] Failed to create geometry index buffer.");
        return 0;
    }

    Geometry.IndexCount = static_cast<uint32>(D3DIndices.size());

    const Rml::CompiledGeometryHandle Handle = NextGeometryHandle++;
    GeometryByHandle.emplace(Handle, Geometry);
    return Handle;
}

void FRmlUiRenderInterfaceD3D11::RenderGeometry(Rml::CompiledGeometryHandle Geometry, Rml::Vector2f Translation, Rml::TextureHandle Texture)
{
    auto It = GeometryByHandle.find(Geometry);
    if (It == GeometryByHandle.end() || !Context)
    {
        return;
    }

    ID3D11ShaderResourceView* SRV = reinterpret_cast<ID3D11ShaderResourceView*>(Texture);
    const bool bUseTexture = SRV != nullptr;
    BindPipeline(bUseTexture);

    FFrameConstants Constants;
    Constants.ViewportSize[0] = ViewportSize.x;
    Constants.ViewportSize[1] = ViewportSize.y;
    Constants.Translation[0] = Translation.x;
    Constants.Translation[1] = Translation.y;
    Constants.RenderScale[0] = RenderScale.x;
    Constants.RenderScale[1] = RenderScale.y;
    Constants.bUseTexture = bUseTexture ? 1u : 0u;
    std::copy(CurrentTransform.data(), CurrentTransform.data() + 16, Constants.Transform);
    Context->UpdateSubresource(ConstantBuffer.Get(), 0, nullptr, &Constants, 0, 0);
    Context->VSSetConstantBuffers(0, 1, ConstantBuffer.GetAddressOf());
    Context->PSSetConstantBuffers(0, 1, ConstantBuffer.GetAddressOf());

    if (bUseTexture)
    {
        Context->PSSetShaderResources(0, 1, &SRV);
    }

    UINT Stride = sizeof(Rml::Vertex);
    UINT Offset = 0;
    ID3D11Buffer* VertexBuffer = It->second.VertexBuffer.Get();
    Context->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
    Context->IASetIndexBuffer(It->second.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    Context->DrawIndexed(It->second.IndexCount, 0, 0);

    if (bUseTexture)
    {
        ID3D11ShaderResourceView* NullSRV = nullptr;
        Context->PSSetShaderResources(0, 1, &NullSRV);
    }
}

void FRmlUiRenderInterfaceD3D11::ReleaseGeometry(Rml::CompiledGeometryHandle Geometry)
{
    GeometryByHandle.erase(Geometry);
}

Rml::TextureHandle FRmlUiRenderInterfaceD3D11::LoadTexture(Rml::Vector2i& TextureDimensions, const Rml::String& Source)
{
    const FString Path = NormalizeRmlUiTexturePath(Source);
    UTexture* Texture = FResourceManager::Get().LoadTexture(Path);
    if (!Texture || !Texture->GetSRV())
    {
        UE_LOG_WARNING("[RmlUi] Failed to load texture: %s", Path.c_str());
        return 0;
    }

    TextureDimensions = Rml::Vector2i(1, 1);
    ID3D11Resource* Resource = nullptr;
    Texture->GetSRV()->GetResource(&Resource);
    if (Resource)
    {
        ID3D11Texture2D* Texture2D = nullptr;
        if (SUCCEEDED(Resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&Texture2D))) && Texture2D)
        {
            D3D11_TEXTURE2D_DESC Desc = {};
            Texture2D->GetDesc(&Desc);
            TextureDimensions = Rml::Vector2i(static_cast<int>(Desc.Width), static_cast<int>(Desc.Height));
            Texture2D->Release();
        }
        Resource->Release();
    }

    return reinterpret_cast<Rml::TextureHandle>(Texture->GetSRV());
}

Rml::TextureHandle FRmlUiRenderInterfaceD3D11::GenerateTexture(Rml::Span<const Rml::byte> Source, Rml::Vector2i SourceDimensions)
{
    if (!Device || Source.empty() || SourceDimensions.x <= 0 || SourceDimensions.y <= 0)
    {
        return 0;
    }

    FGeneratedTexture Generated;

    D3D11_TEXTURE2D_DESC Desc = {};
    Desc.Width = static_cast<UINT>(SourceDimensions.x);
    Desc.Height = static_cast<UINT>(SourceDimensions.y);
    Desc.MipLevels = 1;
    Desc.ArraySize = 1;
    Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    Desc.SampleDesc.Count = 1;
    Desc.Usage = D3D11_USAGE_DEFAULT;
    Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA Data = {};
    Data.pSysMem = Source.data();
    Data.SysMemPitch = static_cast<UINT>(SourceDimensions.x * 4);

    if (FAILED(Device->CreateTexture2D(&Desc, &Data, Generated.Texture.ReleaseAndGetAddressOf())))
    {
        UE_LOG_ERROR("[RmlUi] Failed to generate texture.");
        return 0;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = Desc.Format;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels = 1;

    if (FAILED(Device->CreateShaderResourceView(Generated.Texture.Get(), &SRVDesc, Generated.SRV.ReleaseAndGetAddressOf())))
    {
        UE_LOG_ERROR("[RmlUi] Failed to create generated texture SRV.");
        return 0;
    }

    return MakeGeneratedTextureHandle(Generated.SRV.Get());
}

void FRmlUiRenderInterfaceD3D11::ReleaseTexture(Rml::TextureHandle Texture)
{
    GeneratedTextures.erase(Texture);
}

void FRmlUiRenderInterfaceD3D11::SetTransform(const Rml::Matrix4f* Transform)
{
    CurrentTransform = Transform ? *Transform : Rml::Matrix4f::Identity();
}

void FRmlUiRenderInterfaceD3D11::EnableScissorRegion(bool bEnable)
{
    bScissorEnabled = bEnable;
    ApplyScissorState();
}

void FRmlUiRenderInterfaceD3D11::SetScissorRegion(Rml::Rectanglei Region)
{
    CurrentScissorRegion = Region;
    if (!Context || !bScissorEnabled)
    {
        return;
    }

    D3D11_RECT Rect = {};
    Rect.left = static_cast<LONG>(ViewportMin.x + static_cast<float>(Region.Left()) * RenderScale.x);
    Rect.top = static_cast<LONG>(ViewportMin.y + static_cast<float>(Region.Top()) * RenderScale.y);
    Rect.right = static_cast<LONG>(ViewportMin.x + static_cast<float>(Region.Right()) * RenderScale.x);
    Rect.bottom = static_cast<LONG>(ViewportMin.y + static_cast<float>(Region.Bottom()) * RenderScale.y);
    Context->RSSetScissorRects(1, &Rect);
}

bool FRmlUiRenderInterfaceD3D11::CreateShaders()
{
    TComPtr<ID3DBlob> VSBlob;
    TComPtr<ID3DBlob> PSBlob;
    if (!CompileRmlUiShader("VSMain", "vs_5_0", VSBlob) || !CompileRmlUiShader("PSMain", "ps_5_0", PSBlob))
    {
        return false;
    }

    if (FAILED(Device->CreateVertexShader(VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), nullptr, VertexShader.ReleaseAndGetAddressOf())) ||
        FAILED(Device->CreatePixelShader(PSBlob->GetBufferPointer(), PSBlob->GetBufferSize(), nullptr, PixelShader.ReleaseAndGetAddressOf())))
    {
        UE_LOG_ERROR("[RmlUi] Failed to create shaders.");
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC Elements[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(Rml::Vertex, position)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, static_cast<UINT>(offsetof(Rml::Vertex, colour)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(Rml::Vertex, tex_coord)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    if (FAILED(Device->CreateInputLayout(Elements, ARRAYSIZE(Elements), VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), InputLayout.ReleaseAndGetAddressOf())))
    {
        UE_LOG_ERROR("[RmlUi] Failed to create input layout.");
        return false;
    }

    D3D11_BUFFER_DESC ConstantDesc = {};
    ConstantDesc.ByteWidth = sizeof(FFrameConstants);
    ConstantDesc.Usage = D3D11_USAGE_DEFAULT;
    ConstantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(Device->CreateBuffer(&ConstantDesc, nullptr, ConstantBuffer.ReleaseAndGetAddressOf())))
    {
        UE_LOG_ERROR("[RmlUi] Failed to create constant buffer.");
        return false;
    }

    return true;
}

bool FRmlUiRenderInterfaceD3D11::CreateStates()
{
    D3D11_SAMPLER_DESC SamplerDesc = {};
    SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(Device->CreateSamplerState(&SamplerDesc, SamplerState.ReleaseAndGetAddressOf())))
    {
        return false;
    }

    D3D11_BLEND_DESC BlendDesc = {};
    BlendDesc.RenderTarget[0].BlendEnable = TRUE;
    BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(Device->CreateBlendState(&BlendDesc, BlendState.ReleaseAndGetAddressOf())))
    {
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC DepthDesc = {};
    DepthDesc.DepthEnable = FALSE;
    DepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    DepthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    if (FAILED(Device->CreateDepthStencilState(&DepthDesc, DepthStencilState.ReleaseAndGetAddressOf())))
    {
        return false;
    }

    D3D11_RASTERIZER_DESC RasterDesc = {};
    RasterDesc.FillMode = D3D11_FILL_SOLID;
    RasterDesc.CullMode = D3D11_CULL_NONE;
    RasterDesc.DepthClipEnable = TRUE;
    RasterDesc.ScissorEnable = FALSE;
    if (FAILED(Device->CreateRasterizerState(&RasterDesc, RasterizerState.ReleaseAndGetAddressOf())))
    {
        return false;
    }

    RasterDesc.ScissorEnable = TRUE;
    if (FAILED(Device->CreateRasterizerState(&RasterDesc, ScissorRasterizerState.ReleaseAndGetAddressOf())))
    {
        return false;
    }

    return true;
}

void FRmlUiRenderInterfaceD3D11::BindPipeline(bool bUseTexture)
{
    float BlendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    Context->OMSetBlendState(BlendState.Get(), BlendFactor, 0xffffffff);
    Context->OMSetDepthStencilState(DepthStencilState.Get(), 0);
    Context->RSSetState(bScissorEnabled ? ScissorRasterizerState.Get() : RasterizerState.Get());

    Context->IASetInputLayout(InputLayout.Get());
    Context->VSSetShader(VertexShader.Get(), nullptr, 0);
    Context->PSSetShader(PixelShader.Get(), nullptr, 0);

    if (bUseTexture)
    {
        ID3D11SamplerState* Sampler = SamplerState.Get();
        Context->PSSetSamplers(0, 1, &Sampler);
    }
}

void FRmlUiRenderInterfaceD3D11::ApplyScissorState()
{
    if (!Context)
    {
        return;
    }

    Context->RSSetState(bScissorEnabled ? ScissorRasterizerState.Get() : RasterizerState.Get());
    if (bScissorEnabled && CurrentScissorRegion.Valid())
    {
        SetScissorRegion(CurrentScissorRegion);
    }
}

Rml::TextureHandle FRmlUiRenderInterfaceD3D11::MakeGeneratedTextureHandle(ID3D11ShaderResourceView* SRV)
{
    if (!SRV)
    {
        return 0;
    }

    Rml::TextureHandle Handle = 0;
    do
    {
        Handle = NextGeneratedTextureHandle++;
    }
    while (Handle == 0 || reinterpret_cast<ID3D11ShaderResourceView*>(Handle) != nullptr && GeneratedTextures.contains(Handle));

    // Use the SRV pointer as the actual render handle so RenderGeometry can bind both generated and resource-manager textures uniformly.
    Handle = reinterpret_cast<Rml::TextureHandle>(SRV);
    GeneratedTextures.emplace(Handle, FGeneratedTexture{});
    FGeneratedTexture& Stored = GeneratedTextures[Handle];
    Stored.SRV = SRV;
    ID3D11Resource* Resource = nullptr;
    SRV->GetResource(&Resource);
    if (Resource)
    {
        Resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(Stored.Texture.ReleaseAndGetAddressOf()));
        Resource->Release();
    }
    return Handle;
}
