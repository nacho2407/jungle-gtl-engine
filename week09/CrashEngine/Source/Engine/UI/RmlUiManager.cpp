#include "UI/RmlUiManager.h"

#include "Core/Logging/LogMacros.h"
#include "Platform/Paths.h"
#include "Render/Renderer.h"
#include "Runtime/WindowsWindow.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Input.h>

#include <Windows.h>
#include <d3dcompiler.h>
#include <windowsx.h>

#include <algorithm>
#include <cstring>

namespace
{
const char* GVertexShaderSource = R"(
cbuffer RmlConstants : register(b0)
{
    float4x4 Transform;
};

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

VSOutput main(VSInput Input)
{
    VSOutput Output;
    Output.Position = mul(float4(Input.Position.xy, 0.0f, 1.0f), Transform);
    Output.Color = Input.Color;
    Output.TexCoord = Input.TexCoord;
    return Output;
}
)";

const char* GPixelShaderSource = R"(
Texture2D RmlTexture : register(t0);
SamplerState RmlSampler : register(s0);

struct PSInput
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR0;
    float2 TexCoord : TEXCOORD0;
};

float4 main(PSInput Input) : SV_TARGET
{
    float4 TextureColor = RmlTexture.Sample(RmlSampler, Input.TexCoord);
    return Input.Color * TextureColor;
}
)";

template <typename T>
void SafeRelease(T*& Resource)
{
    if (Resource)
    {
        Resource->Release();
        Resource = nullptr;
    }
}

Rml::String ToRmlPath(const std::wstring& Path)
{
    return FPaths::ToUtf8(Path);
}

void BuildOrthographicMatrix(float* Out, uint32 Width, uint32 Height)
{
    const float L = 0.0f;
    const float R = static_cast<float>(Width);
    const float T = 0.0f;
    const float B = static_cast<float>(Height);

    std::fill(Out, Out + 16, 0.0f);
    Out[0] = 2.0f / (R - L);
    Out[5] = 2.0f / (T - B);
    Out[10] = 0.5f;
    Out[12] = (R + L) / (L - R);
    Out[13] = (T + B) / (B - T);
    Out[15] = 1.0f;
}

Rml::Input::KeyIdentifier TranslateBasicVirtualKey(WPARAM VirtualKey)
{
    using namespace Rml::Input;

    if (VirtualKey >= '0' && VirtualKey <= '9')
    {
        return static_cast<KeyIdentifier>(KI_0 + (VirtualKey - '0'));
    }
    if (VirtualKey >= 'A' && VirtualKey <= 'Z')
    {
        return static_cast<KeyIdentifier>(KI_A + (VirtualKey - 'A'));
    }
    if (VirtualKey >= VK_F1 && VirtualKey <= VK_F12)
    {
        return static_cast<KeyIdentifier>(KI_F1 + (VirtualKey - VK_F1));
    }

    switch (VirtualKey)
    {
    case VK_SPACE: return KI_SPACE;
    case VK_BACK: return KI_BACK;
    case VK_TAB: return KI_TAB;
    case VK_RETURN: return KI_RETURN;
    case VK_ESCAPE: return KI_ESCAPE;
    case VK_PRIOR: return KI_PRIOR;
    case VK_NEXT: return KI_NEXT;
    case VK_END: return KI_END;
    case VK_HOME: return KI_HOME;
    case VK_LEFT: return KI_LEFT;
    case VK_UP: return KI_UP;
    case VK_RIGHT: return KI_RIGHT;
    case VK_DOWN: return KI_DOWN;
    case VK_INSERT: return KI_INSERT;
    case VK_DELETE: return KI_DELETE;
    case VK_SHIFT: return KI_LSHIFT;
    case VK_CONTROL: return KI_LCONTROL;
    case VK_MENU: return KI_LMENU;
    case VK_OEM_PLUS: return KI_OEM_PLUS;
    case VK_OEM_COMMA: return KI_OEM_COMMA;
    case VK_OEM_MINUS: return KI_OEM_MINUS;
    case VK_OEM_PERIOD: return KI_OEM_PERIOD;
    case VK_OEM_1: return KI_OEM_1;
    case VK_OEM_2: return KI_OEM_2;
    case VK_OEM_3: return KI_OEM_3;
    case VK_OEM_4: return KI_OEM_4;
    case VK_OEM_5: return KI_OEM_5;
    case VK_OEM_6: return KI_OEM_6;
    case VK_OEM_7: return KI_OEM_7;
    default: return KI_UNKNOWN;
    }
}
} // namespace

double FRmlUiSystemInterface::GetElapsedTime()
{
    static LARGE_INTEGER Frequency = {};
    static LARGE_INTEGER Start = {};

    if (Frequency.QuadPart == 0)
    {
        QueryPerformanceFrequency(&Frequency);
        QueryPerformanceCounter(&Start);
    }

    LARGE_INTEGER Now = {};
    QueryPerformanceCounter(&Now);
    return static_cast<double>(Now.QuadPart - Start.QuadPart) / static_cast<double>(Frequency.QuadPart);
}

bool FRmlUiSystemInterface::LogMessage(Rml::Log::Type Type, const Rml::String& Message)
{
    const ELogLevel Level = (Type <= Rml::Log::LT_WARNING) ? ELogLevel::Warning : ELogLevel::Info;
    ::LogMessage(Level, "Engine", "RmlUi: %s", Message.c_str());
    return true;
}

bool FRmlUiNullFontEngine::LoadFontFace(const Rml::String&, int, bool, Rml::Style::FontWeight)
{
    UE_LOG(Engine, Warning, "RmlUi font loading is disabled. Add a real FontEngineInterface or enable FreeType to render text.");
    return false;
}

bool FRmlUiNullFontEngine::LoadFontFace(const Rml::String&, int, const Rml::String&, Rml::Style::FontStyle, Rml::Style::FontWeight, bool)
{
    UE_LOG(Engine, Warning, "RmlUi font loading is disabled. Add a real FontEngineInterface or enable FreeType to render text.");
    return false;
}

bool FRmlUiNullFontEngine::LoadFontFace(Rml::Span<const Rml::byte>, int, const Rml::String&, Rml::Style::FontStyle, Rml::Style::FontWeight, bool)
{
    UE_LOG(Engine, Warning, "RmlUi font loading is disabled. Add a real FontEngineInterface or enable FreeType to render text.");
    return false;
}

bool FRmlUiD3D11RenderInterface::Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InContext)
{
    Device = InDevice;
    Context = InContext;
    return CreateDeviceResources();
}

void FRmlUiD3D11RenderInterface::Shutdown()
{
    for (auto& Pair : Geometries)
    {
        SafeRelease(Pair.second.VertexBuffer);
        SafeRelease(Pair.second.IndexBuffer);
    }
    Geometries.clear();

    for (auto& Pair : Textures)
    {
        SafeRelease(Pair.second.ShaderResourceView);
    }
    Textures.clear();

    ReleaseDeviceResources();
    Device = nullptr;
    Context = nullptr;
}

void FRmlUiD3D11RenderInterface::SetViewportSize(uint32 Width, uint32 Height)
{
    ViewportWidth = std::max(Width, 1u);
    ViewportHeight = std::max(Height, 1u);
}

Rml::CompiledGeometryHandle FRmlUiD3D11RenderInterface::CompileGeometry(Rml::Span<const Rml::Vertex> Vertices, Rml::Span<const int> Indices)
{
    if (!Device || Vertices.empty() || Indices.empty())
    {
        return 0;
    }

    std::vector<FVertex> ConvertedVertices;
    ConvertedVertices.reserve(Vertices.size());
    for (const Rml::Vertex& Vertex : Vertices)
    {
        FVertex OutVertex = {};
        OutVertex.Position[0] = Vertex.position.x;
        OutVertex.Position[1] = Vertex.position.y;
        OutVertex.Color[0] = Vertex.colour.red;
        OutVertex.Color[1] = Vertex.colour.green;
        OutVertex.Color[2] = Vertex.colour.blue;
        OutVertex.Color[3] = Vertex.colour.alpha;
        OutVertex.TexCoord[0] = Vertex.tex_coord.x;
        OutVertex.TexCoord[1] = Vertex.tex_coord.y;
        ConvertedVertices.push_back(OutVertex);
    }

    D3D11_BUFFER_DESC VertexBufferDesc = {};
    VertexBufferDesc.ByteWidth = static_cast<UINT>(ConvertedVertices.size() * sizeof(FVertex));
    VertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA VertexData = {};
    VertexData.pSysMem = ConvertedVertices.data();

    FCompiledGeometry Geometry = {};
    if (FAILED(Device->CreateBuffer(&VertexBufferDesc, &VertexData, &Geometry.VertexBuffer)))
    {
        return 0;
    }

    std::vector<uint32> ConvertedIndices;
    ConvertedIndices.reserve(Indices.size());
    for (int Index : Indices)
    {
        ConvertedIndices.push_back(static_cast<uint32>(Index));
    }

    D3D11_BUFFER_DESC IndexBufferDesc = {};
    IndexBufferDesc.ByteWidth = static_cast<UINT>(ConvertedIndices.size() * sizeof(uint32));
    IndexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA IndexData = {};
    IndexData.pSysMem = ConvertedIndices.data();

    if (FAILED(Device->CreateBuffer(&IndexBufferDesc, &IndexData, &Geometry.IndexBuffer)))
    {
        SafeRelease(Geometry.VertexBuffer);
        return 0;
    }

    Geometry.IndexCount = static_cast<uint32>(ConvertedIndices.size());

    const uint64 Handle = AllocateHandle();
    Geometries.emplace(Handle, Geometry);
    return static_cast<Rml::CompiledGeometryHandle>(Handle);
}

void FRmlUiD3D11RenderInterface::RenderGeometry(Rml::CompiledGeometryHandle GeometryHandle, Rml::Vector2f Translation, Rml::TextureHandle TextureHandle)
{
    auto GeometryIt = Geometries.find(static_cast<uint64>(GeometryHandle));
    if (GeometryIt == Geometries.end() || !Context)
    {
        return;
    }

    UpdateConstants(Translation);

    ID3D11ShaderResourceView* SRV = WhiteTexture;
    auto TextureIt = Textures.find(static_cast<uint64>(TextureHandle));
    if (TextureIt != Textures.end())
    {
        SRV = TextureIt->second.ShaderResourceView;
    }

    const FCompiledGeometry& Geometry = GeometryIt->second;
    UINT Stride = sizeof(FVertex);
    UINT Offset = 0;

    Context->IASetInputLayout(InputLayout);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context->IASetVertexBuffers(0, 1, &Geometry.VertexBuffer, &Stride, &Offset);
    Context->IASetIndexBuffer(Geometry.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

    Context->VSSetShader(VertexShader, nullptr, 0);
    Context->VSSetConstantBuffers(0, 1, &ConstantBuffer);
    Context->PSSetShader(PixelShader, nullptr, 0);
    Context->PSSetShaderResources(0, 1, &SRV);
    Context->PSSetSamplers(0, 1, &SamplerState);

    const float BlendFactor[4] = {};
    Context->OMSetBlendState(BlendState, BlendFactor, 0xffffffff);
    Context->OMSetDepthStencilState(DepthStencilState, 0);
    Context->RSSetState(bScissorEnabled ? ScissorRasterizerState : RasterizerState);

    Context->DrawIndexed(Geometry.IndexCount, 0, 0);

    ID3D11ShaderResourceView* NullSRV = nullptr;
    Context->PSSetShaderResources(0, 1, &NullSRV);
}

void FRmlUiD3D11RenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle GeometryHandle)
{
    auto It = Geometries.find(static_cast<uint64>(GeometryHandle));
    if (It == Geometries.end())
    {
        return;
    }

    SafeRelease(It->second.VertexBuffer);
    SafeRelease(It->second.IndexBuffer);
    Geometries.erase(It);
}

Rml::TextureHandle FRmlUiD3D11RenderInterface::LoadTexture(Rml::Vector2i& TextureDimensions, const Rml::String&)
{
    TextureDimensions = {0, 0};
    return 0;
}

Rml::TextureHandle FRmlUiD3D11RenderInterface::GenerateTexture(Rml::Span<const Rml::byte> Source, Rml::Vector2i SourceDimensions)
{
    if (!Device || Source.empty() || SourceDimensions.x <= 0 || SourceDimensions.y <= 0)
    {
        return 0;
    }

    D3D11_TEXTURE2D_DESC TextureDesc = {};
    TextureDesc.Width = static_cast<UINT>(SourceDimensions.x);
    TextureDesc.Height = static_cast<UINT>(SourceDimensions.y);
    TextureDesc.MipLevels = 1;
    TextureDesc.ArraySize = 1;
    TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    TextureDesc.SampleDesc.Count = 1;
    TextureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA TextureData = {};
    TextureData.pSysMem = Source.data();
    TextureData.SysMemPitch = static_cast<UINT>(SourceDimensions.x * 4);

    ID3D11Texture2D* Texture = nullptr;
    if (FAILED(Device->CreateTexture2D(&TextureDesc, &TextureData, &Texture)))
    {
        return 0;
    }

    FTexture StoredTexture = {};
    if (FAILED(Device->CreateShaderResourceView(Texture, nullptr, &StoredTexture.ShaderResourceView)))
    {
        Texture->Release();
        return 0;
    }
    Texture->Release();

    StoredTexture.Width = static_cast<uint32>(SourceDimensions.x);
    StoredTexture.Height = static_cast<uint32>(SourceDimensions.y);

    const uint64 Handle = AllocateHandle();
    Textures.emplace(Handle, StoredTexture);
    return static_cast<Rml::TextureHandle>(Handle);
}

void FRmlUiD3D11RenderInterface::ReleaseTexture(Rml::TextureHandle TextureHandle)
{
    auto It = Textures.find(static_cast<uint64>(TextureHandle));
    if (It == Textures.end())
    {
        return;
    }

    SafeRelease(It->second.ShaderResourceView);
    Textures.erase(It);
}

void FRmlUiD3D11RenderInterface::EnableScissorRegion(bool Enable)
{
    bScissorEnabled = Enable;
}

void FRmlUiD3D11RenderInterface::SetScissorRegion(Rml::Rectanglei Region)
{
    if (!Context)
    {
        return;
    }

    D3D11_RECT Rect = {};
    Rect.left = Region.Left();
    Rect.top = Region.Top();
    Rect.right = Region.Right();
    Rect.bottom = Region.Bottom();
    Context->RSSetScissorRects(1, &Rect);
}

void FRmlUiD3D11RenderInterface::SetTransform(const Rml::Matrix4f* Transform)
{
    if (Transform)
    {
        CurrentTransform = *Transform;
        bHasTransform = true;
    }
    else
    {
        CurrentTransform = Rml::Matrix4f::Identity();
        bHasTransform = false;
    }
}

bool FRmlUiD3D11RenderInterface::CreateDeviceResources()
{
    if (!Device)
    {
        return false;
    }

    ID3DBlob* VertexShaderBlob = nullptr;
    ID3DBlob* PixelShaderBlob = nullptr;
    ID3DBlob* ErrorBlob = nullptr;

    UINT CompileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if _DEBUG
    CompileFlags |= D3DCOMPILE_DEBUG;
#endif

    HRESULT Result = D3DCompile(GVertexShaderSource, std::strlen(GVertexShaderSource), nullptr, nullptr, nullptr, "main", "vs_5_0", CompileFlags, 0, &VertexShaderBlob, &ErrorBlob);
    if (FAILED(Result))
    {
        if (ErrorBlob)
        {
            UE_LOG(Engine, Error, "RmlUi vertex shader compile failed: %s", static_cast<const char*>(ErrorBlob->GetBufferPointer()));
            ErrorBlob->Release();
        }
        return false;
    }

    Result = D3DCompile(GPixelShaderSource, std::strlen(GPixelShaderSource), nullptr, nullptr, nullptr, "main", "ps_5_0", CompileFlags, 0, &PixelShaderBlob, &ErrorBlob);
    if (FAILED(Result))
    {
        if (ErrorBlob)
        {
            UE_LOG(Engine, Error, "RmlUi pixel shader compile failed: %s", static_cast<const char*>(ErrorBlob->GetBufferPointer()));
            ErrorBlob->Release();
        }
        VertexShaderBlob->Release();
        return false;
    }

    Result = Device->CreateVertexShader(VertexShaderBlob->GetBufferPointer(), VertexShaderBlob->GetBufferSize(), nullptr, &VertexShader);
    if (SUCCEEDED(Result))
    {
        Result = Device->CreatePixelShader(PixelShaderBlob->GetBufferPointer(), PixelShaderBlob->GetBufferSize(), nullptr, &PixelShader);
    }

    D3D11_INPUT_ELEMENT_DESC Layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(FVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    if (SUCCEEDED(Result))
    {
        Result = Device->CreateInputLayout(Layout, ARRAYSIZE(Layout), VertexShaderBlob->GetBufferPointer(), VertexShaderBlob->GetBufferSize(), &InputLayout);
    }

    VertexShaderBlob->Release();
    PixelShaderBlob->Release();

    D3D11_BUFFER_DESC ConstantBufferDesc = {};
    ConstantBufferDesc.ByteWidth = sizeof(FConstants);
    ConstantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    ConstantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ConstantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (SUCCEEDED(Result))
    {
        Result = Device->CreateBuffer(&ConstantBufferDesc, nullptr, &ConstantBuffer);
    }

    D3D11_SAMPLER_DESC SamplerDesc = {};
    SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    if (SUCCEEDED(Result))
    {
        Result = Device->CreateSamplerState(&SamplerDesc, &SamplerState);
    }

    const uint32 WhitePixel = 0xffffffff;
    D3D11_TEXTURE2D_DESC WhiteTextureDesc = {};
    WhiteTextureDesc.Width = 1;
    WhiteTextureDesc.Height = 1;
    WhiteTextureDesc.MipLevels = 1;
    WhiteTextureDesc.ArraySize = 1;
    WhiteTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    WhiteTextureDesc.SampleDesc.Count = 1;
    WhiteTextureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    WhiteTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA WhiteTextureData = {};
    WhiteTextureData.pSysMem = &WhitePixel;
    WhiteTextureData.SysMemPitch = sizeof(WhitePixel);

    ID3D11Texture2D* WhiteTextureResource = nullptr;
    if (SUCCEEDED(Result))
    {
        Result = Device->CreateTexture2D(&WhiteTextureDesc, &WhiteTextureData, &WhiteTextureResource);
    }
    if (SUCCEEDED(Result))
    {
        Result = Device->CreateShaderResourceView(WhiteTextureResource, nullptr, &WhiteTexture);
    }
    SafeRelease(WhiteTextureResource);

    D3D11_BLEND_DESC BlendDesc = {};
    BlendDesc.RenderTarget[0].BlendEnable = TRUE;
    BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (SUCCEEDED(Result))
    {
        Result = Device->CreateBlendState(&BlendDesc, &BlendState);
    }

    D3D11_RASTERIZER_DESC RasterizerDesc = {};
    RasterizerDesc.FillMode = D3D11_FILL_SOLID;
    RasterizerDesc.CullMode = D3D11_CULL_NONE;
    RasterizerDesc.DepthClipEnable = TRUE;
    if (SUCCEEDED(Result))
    {
        Result = Device->CreateRasterizerState(&RasterizerDesc, &RasterizerState);
    }
    RasterizerDesc.ScissorEnable = TRUE;
    if (SUCCEEDED(Result))
    {
        Result = Device->CreateRasterizerState(&RasterizerDesc, &ScissorRasterizerState);
    }

    D3D11_DEPTH_STENCIL_DESC DepthDesc = {};
    DepthDesc.DepthEnable = FALSE;
    DepthDesc.StencilEnable = FALSE;
    if (SUCCEEDED(Result))
    {
        Result = Device->CreateDepthStencilState(&DepthDesc, &DepthStencilState);
    }

    if (FAILED(Result))
    {
        ReleaseDeviceResources();
        return false;
    }

    return true;
}

void FRmlUiD3D11RenderInterface::ReleaseDeviceResources()
{
    SafeRelease(VertexShader);
    SafeRelease(PixelShader);
    SafeRelease(InputLayout);
    SafeRelease(ConstantBuffer);
    SafeRelease(SamplerState);
    SafeRelease(WhiteTexture);
    SafeRelease(BlendState);
    SafeRelease(RasterizerState);
    SafeRelease(ScissorRasterizerState);
    SafeRelease(DepthStencilState);
}

void FRmlUiD3D11RenderInterface::UpdateConstants(Rml::Vector2f Translation)
{
    if (!Context || !ConstantBuffer)
    {
        return;
    }

    FConstants Constants = {};
    BuildOrthographicMatrix(Constants.Transform, ViewportWidth, ViewportHeight);

    Constants.Transform[12] += Translation.x * Constants.Transform[0];
    Constants.Transform[13] += Translation.y * Constants.Transform[5];

    if (bHasTransform)
    {
        const float* M = CurrentTransform.data();
        (void)M;
        // The base orthographic path is kept active for now. RmlUi transform support can be expanded here.
    }

    D3D11_MAPPED_SUBRESOURCE Mapped = {};
    if (SUCCEEDED(Context->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
    {
        std::memcpy(Mapped.pData, &Constants, sizeof(Constants));
        Context->Unmap(ConstantBuffer, 0);
    }
}

uint64 FRmlUiD3D11RenderInterface::AllocateHandle()
{
    return NextHandle++;
}

bool FRmlUiManager::Initialize(FWindowsWindow* Window, FRenderer& Renderer)
{
    if (bInitialized || !Window)
    {
        return bInitialized;
    }

    ID3D11Device* Device = Renderer.GetFD3DDevice().GetDevice();
    ID3D11DeviceContext* DeviceContext = Renderer.GetFD3DDevice().GetDeviceContext();
    if (!RenderInterface.Initialize(Device, DeviceContext))
    {
        UE_LOG(Engine, Error, "Failed to initialize RmlUi D3D11 render interface.");
        return false;
    }

    const uint32 Width = static_cast<uint32>(std::max(Window->GetWidth(), 1.0f));
    const uint32 Height = static_cast<uint32>(std::max(Window->GetHeight(), 1.0f));
    RenderInterface.SetViewportSize(Width, Height);

    Rml::SetSystemInterface(&SystemInterface);
    Rml::SetRenderInterface(&RenderInterface);
    Rml::SetFontEngineInterface(&FontEngine);

    if (!Rml::Initialise())
    {
        RenderInterface.Shutdown();
        return false;
    }

    Context = Rml::CreateContext("Main", Rml::Vector2i(static_cast<int>(Width), static_cast<int>(Height)));
    if (!Context)
    {
        Rml::Shutdown();
        RenderInterface.Shutdown();
        return false;
    }

    bInitialized = true;
    LoadDefaultDocument();
    UE_LOG(Engine, Info, "RmlUi initialized.");
    return true;
}

void FRmlUiManager::Shutdown()
{
    if (!bInitialized)
    {
        return;
    }

    Context = nullptr;
    Rml::Shutdown();
    RenderInterface.Shutdown();
    bInitialized = false;
}

void FRmlUiManager::Update()
{
    if (Context)
    {
        Context->Update();
    }
}

void FRmlUiManager::Render()
{
    if (Context)
    {
        Context->Render();
    }
}

void FRmlUiManager::OnWindowResized(uint32 Width, uint32 Height)
{
    if (Width == 0 || Height == 0)
    {
        return;
    }

    RenderInterface.SetViewportSize(Width, Height);
    if (Context)
    {
        Context->SetDimensions(Rml::Vector2i(static_cast<int>(Width), static_cast<int>(Height)));
    }
}

bool FRmlUiManager::HandleWindowMessage(HWND__* WindowHandle, unsigned int Message, WPARAM WParam, LPARAM LParam)
{
    if (!Context)
    {
        return false;
    }

    const int Modifiers = GetKeyModifierState();

    switch (Message)
    {
    case WM_MOUSEMOVE:
        return Context->ProcessMouseMove(GET_X_LPARAM(LParam), GET_Y_LPARAM(LParam), Modifiers);
    case WM_LBUTTONDOWN:
        SetCapture(WindowHandle);
        return Context->ProcessMouseButtonDown(0, Modifiers);
    case WM_LBUTTONUP:
        ReleaseCapture();
        return Context->ProcessMouseButtonUp(0, Modifiers);
    case WM_RBUTTONDOWN:
        return Context->ProcessMouseButtonDown(1, Modifiers);
    case WM_RBUTTONUP:
        return Context->ProcessMouseButtonUp(1, Modifiers);
    case WM_MBUTTONDOWN:
        return Context->ProcessMouseButtonDown(2, Modifiers);
    case WM_MBUTTONUP:
        return Context->ProcessMouseButtonUp(2, Modifiers);
    case WM_MOUSEWHEEL:
        return Context->ProcessMouseWheel(static_cast<float>(GET_WHEEL_DELTA_WPARAM(WParam)) / static_cast<float>(WHEEL_DELTA), Modifiers);
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        return Context->ProcessKeyDown(TranslateKey(WParam, LParam), Modifiers);
    case WM_KEYUP:
    case WM_SYSKEYUP:
        return Context->ProcessKeyUp(TranslateKey(WParam, LParam), Modifiers);
    case WM_CHAR:
        if (WParam >= 32)
        {
            return Context->ProcessTextInput(static_cast<Rml::Character>(WParam));
        }
        break;
    default:
        break;
    }

    return false;
}

void FRmlUiManager::LoadDefaultDocument()
{
    if (!Context)
    {
        return;
    }

    const std::wstring DefaultDocumentPath = FPaths::Combine(FPaths::ContentDir(), L"UI/Default.rml");
    Rml::ElementDocument* Document = Context->LoadDocument(ToRmlPath(DefaultDocumentPath));
    if (Document)
    {
        Document->Show();
    }
}

int FRmlUiManager::GetKeyModifierState() const
{
    int Modifiers = 0;
    if (GetKeyState(VK_CONTROL) & 0x8000)
    {
        Modifiers |= Rml::Input::KM_CTRL;
    }
    if (GetKeyState(VK_SHIFT) & 0x8000)
    {
        Modifiers |= Rml::Input::KM_SHIFT;
    }
    if (GetKeyState(VK_MENU) & 0x8000)
    {
        Modifiers |= Rml::Input::KM_ALT;
    }
    if (GetKeyState(VK_CAPITAL) & 0x0001)
    {
        Modifiers |= Rml::Input::KM_CAPSLOCK;
    }
    if (GetKeyState(VK_NUMLOCK) & 0x0001)
    {
        Modifiers |= Rml::Input::KM_NUMLOCK;
    }
    return Modifiers;
}

Rml::Input::KeyIdentifier FRmlUiManager::TranslateKey(WPARAM VirtualKey, LPARAM KeyData) const
{
    (void)KeyData;
    return TranslateBasicVirtualKey(VirtualKey);
}
