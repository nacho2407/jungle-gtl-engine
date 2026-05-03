#include "UI/RmlUiManager.h"

#include "Core/Logging/LogMacros.h"
#include "Platform/Paths.h"
#include "Render/Renderer.h"
#include "Runtime/WindowsWindow.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Input.h>

#include <WICTextureLoader.h>
#include <Windows.h>
#include <d3dcompiler.h>
#include <windowsx.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

namespace
{
const char* GVertexShaderSource = R"(
cbuffer RmlConstants : register(b0)
{
    row_major float4x4 Transform;
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

std::string FormatFloat(float Value)
{
    char Buffer[64];
    std::snprintf(Buffer, sizeof(Buffer), "%.3f", Value);
    return Buffer;
}

std::string FormatPx(float Value)
{
    return FormatFloat(Value) + "px";
}

std::filesystem::path ResolveRmlFilePath(const Rml::String& Path)
{
    if (Path.empty())
    {
        return {};
    }

    Rml::String Normalized = Path;
    std::replace(Normalized.begin(), Normalized.end(), '\\', '/');

    const bool bLooksLikeWindowsAbsolute = Normalized.size() > 1 && Normalized[1] == ':';
    if (!bLooksLikeWindowsAbsolute && !Normalized.empty() && Normalized[0] == '/')
    {
        Normalized.erase(Normalized.begin());
    }

    std::filesystem::path FilePath = FPaths::ToPath(Normalized);
    if (!FilePath.is_absolute())
    {
        FilePath = std::filesystem::path(FPaths::RootDir()) / FilePath;
    }

    return FilePath.lexically_normal();
}

std::filesystem::path ResolveRmlTextureFilePath(const Rml::String& Source)
{
    std::filesystem::path TexturePath = ResolveRmlFilePath(Source);
    if (std::filesystem::exists(TexturePath))
    {
        return TexturePath.lexically_normal();
    }

    Rml::String Normalized = Source;
    std::replace(Normalized.begin(), Normalized.end(), '\\', '/');
    while (!Normalized.empty() && Normalized[0] == '/')
    {
        Normalized.erase(Normalized.begin());
    }

    const std::filesystem::path SourcePath = FPaths::ToPath(Normalized);
    const std::filesystem::path RootCandidate = (std::filesystem::path(FPaths::RootDir()) / SourcePath).lexically_normal();
    if (std::filesystem::exists(RootCandidate))
    {
        return RootCandidate;
    }

    const std::filesystem::path ContentCandidate = (std::filesystem::path(FPaths::ContentDir()) / SourcePath).lexically_normal();
    if (std::filesystem::exists(ContentCandidate))
    {
        return ContentCandidate;
    }

    const std::filesystem::path AssetCandidate = (std::filesystem::path(FPaths::AssetDir()) / SourcePath).lexically_normal();
    if (std::filesystem::exists(AssetCandidate))
    {
        return AssetCandidate;
    }

    return TexturePath.lexically_normal();
}

std::string ResolveRmlTextureSource(const FString& DocumentPath, const FString& TexturePath)
{
    if (TexturePath.empty())
    {
        return {};
    }

    std::filesystem::path Path = FPaths::ToPath(TexturePath);
    if (Path.is_absolute())
    {
        return FPaths::ToUtf8(Path.lexically_normal().wstring());
    }

    const std::filesystem::path ContentPath = (std::filesystem::path(FPaths::ContentDir()) / Path).lexically_normal();
    if (std::filesystem::exists(ContentPath))
    {
        return "/" + FPaths::MakeRelativeToRoot(ContentPath);
    }

    const FString AssetRelativePath = FPaths::ResolveAssetPath(DocumentPath, TexturePath);
    const std::filesystem::path RootRelativePath = FPaths::ToPath(AssetRelativePath);
    const std::filesystem::path RootPath = (std::filesystem::path(FPaths::RootDir()) / RootRelativePath).lexically_normal();
    if (std::filesystem::exists(RootPath))
    {
        return "/" + FPaths::MakeRelativeToRoot(RootPath);
    }

    return TexturePath;
}

std::string CreateRmlTextBitmapSource(const FString& Text)
{
    const std::wstring WideText = FPaths::ToWide(Text);
    if (WideText.empty())
    {
        return {};
    }

    constexpr int FontSize = 32;
    HDC ScreenDC = GetDC(nullptr);
    HDC MemoryDC = CreateCompatibleDC(ScreenDC);
    HFONT Font = CreateFontW(-FontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Malgun Gothic");
    HGDIOBJ OldFont = SelectObject(MemoryDC, Font);

    SIZE TextSize = {};
    GetTextExtentPoint32W(MemoryDC, WideText.c_str(), static_cast<int>(WideText.size()), &TextSize);
    const int Width = std::max(TextSize.cx + 8, 1L);
    const int Height = std::max(FontSize + 12, 1);

    BITMAPINFO BitmapInfo = {};
    BitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    BitmapInfo.bmiHeader.biWidth = Width;
    BitmapInfo.bmiHeader.biHeight = -Height;
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = 32;
    BitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* Bits = nullptr;
    HBITMAP Bitmap = CreateDIBSection(MemoryDC, &BitmapInfo, DIB_RGB_COLORS, &Bits, nullptr, 0);
    if (!Bitmap || !Bits)
    {
        SelectObject(MemoryDC, OldFont);
        DeleteObject(Font);
        if (Bitmap)
        {
            DeleteObject(Bitmap);
        }
        DeleteDC(MemoryDC);
        ReleaseDC(nullptr, ScreenDC);
        return {};
    }

    HGDIOBJ OldBitmap = SelectObject(MemoryDC, Bitmap);
    RECT TextRect = {0, 0, Width, Height};
    SetBkMode(MemoryDC, TRANSPARENT);
    SetTextColor(MemoryDC, RGB(255, 255, 255));
    DrawTextW(MemoryDC, WideText.c_str(), static_cast<int>(WideText.size()), &TextRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    std::vector<uint8> Pixels(static_cast<size_t>(Width) * static_cast<size_t>(Height) * 4);
    const uint8* Source = static_cast<const uint8*>(Bits);
    for (int Y = 0; Y < Height; ++Y)
    {
        for (int X = 0; X < Width; ++X)
        {
            const size_t SourceIndex = (static_cast<size_t>(Y) * Width + X) * 4;
            const uint8 B = Source[SourceIndex + 0];
            const uint8 G = Source[SourceIndex + 1];
            const uint8 R = Source[SourceIndex + 2];
            const uint8 A = std::max(R, std::max(G, B));
            const int FlippedY = Height - 1 - Y;
            const size_t TargetIndex = (static_cast<size_t>(FlippedY) * Width + X) * 4;
            Pixels[TargetIndex + 0] = B;
            Pixels[TargetIndex + 1] = G;
            Pixels[TargetIndex + 2] = R;
            Pixels[TargetIndex + 3] = A;
        }
    }

    SelectObject(MemoryDC, OldBitmap);
    SelectObject(MemoryDC, OldFont);
    DeleteObject(Bitmap);
    DeleteObject(Font);
    DeleteDC(MemoryDC);
    ReleaseDC(nullptr, ScreenDC);

    const std::filesystem::path OutputDir = std::filesystem::path(FPaths::SavedDir()) / L"RmlUiText";
    std::filesystem::create_directories(OutputDir);

    const size_t HashValue = std::hash<std::string>{}(Text);
    const std::filesystem::path OutputPath = OutputDir / (L"text_" + std::to_wstring(static_cast<unsigned long long>(HashValue)) + L".bmp");

#pragma pack(push, 1)
    struct FBitmapFileHeader
    {
        uint16 Type = 0x4D42;
        uint32 Size = 0;
        uint16 Reserved1 = 0;
        uint16 Reserved2 = 0;
        uint32 OffBits = 0;
    };

    struct FBitmapInfoHeader
    {
        uint32 Size = 40;
        int32 Width = 0;
        int32 Height = 0;
        uint16 Planes = 1;
        uint16 BitCount = 32;
        uint32 Compression = BI_RGB;
        uint32 SizeImage = 0;
        int32 XPelsPerMeter = 0;
        int32 YPelsPerMeter = 0;
        uint32 ClrUsed = 0;
        uint32 ClrImportant = 0;
    };
#pragma pack(pop)

    FBitmapFileHeader FileHeader;
    FBitmapInfoHeader InfoHeader;
    InfoHeader.Width = Width;
    InfoHeader.Height = Height;
    InfoHeader.SizeImage = static_cast<uint32>(Pixels.size());
    FileHeader.OffBits = sizeof(FBitmapFileHeader) + sizeof(FBitmapInfoHeader);
    FileHeader.Size = FileHeader.OffBits + InfoHeader.SizeImage;

    FILE* File = nullptr;
    if (_wfopen_s(&File, OutputPath.c_str(), L"wb") != 0 || !File)
    {
        return {};
    }

    std::fwrite(&FileHeader, sizeof(FileHeader), 1, File);
    std::fwrite(&InfoHeader, sizeof(InfoHeader), 1, File);
    std::fwrite(Pixels.data(), 1, Pixels.size(), File);
    std::fclose(File);

    return "/" + FPaths::MakeRelativeToRoot(OutputPath);
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
    return false;
}

bool FRmlUiNullFontEngine::LoadFontFace(const Rml::String&, int, const Rml::String&, Rml::Style::FontStyle, Rml::Style::FontWeight, bool)
{
    return false;
}

bool FRmlUiNullFontEngine::LoadFontFace(Rml::Span<const Rml::byte>, int, const Rml::String&, Rml::Style::FontStyle, Rml::Style::FontWeight, bool)
{
    return false;
}

Rml::FileHandle FRmlUiFileInterface::Open(const Rml::String& Path)
{
    const std::filesystem::path FilePath = ResolveRmlFilePath(Path);
    FILE* File = nullptr;
    if (_wfopen_s(&File, FilePath.c_str(), L"rb") != 0)
    {
        File = nullptr;
    }
    return reinterpret_cast<Rml::FileHandle>(File);
}

void FRmlUiFileInterface::Close(Rml::FileHandle File)
{
    if (File)
    {
        std::fclose(reinterpret_cast<FILE*>(File));
    }
}

size_t FRmlUiFileInterface::Read(void* Buffer, size_t Size, Rml::FileHandle File)
{
    return File ? std::fread(Buffer, 1, Size, reinterpret_cast<FILE*>(File)) : 0;
}

bool FRmlUiFileInterface::Seek(Rml::FileHandle File, long Offset, int Origin)
{
    return File && _fseeki64(reinterpret_cast<FILE*>(File), Offset, Origin) == 0;
}

size_t FRmlUiFileInterface::Tell(Rml::FileHandle File)
{
    if (!File)
    {
        return 0;
    }

    const __int64 Position = _ftelli64(reinterpret_cast<FILE*>(File));
    return Position >= 0 ? static_cast<size_t>(Position) : 0;
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
        UE_LOG(Engine, Warning, "RmlUi CompileGeometry skipped. device=%p vertices=%zu indices=%zu", Device, Vertices.size(), Indices.size());
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
        UE_LOG(Engine, Warning, "RmlUi RenderGeometry skipped. geometry=%llu found=%d context=%p", static_cast<uint64>(GeometryHandle), GeometryIt != Geometries.end() ? 1 : 0, Context);
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

Rml::TextureHandle FRmlUiD3D11RenderInterface::LoadTexture(Rml::Vector2i& TextureDimensions, const Rml::String& Source)
{
    TextureDimensions = {0, 0};
    if (!Device || Source.empty())
    {
        UE_LOG(Engine, Warning, "RmlUi LoadTexture skipped. device=%p source=%s", Device, Source.c_str());
        return 0;
    }

    std::filesystem::path TexturePath = ResolveRmlTextureFilePath(Source);
    const bool bExists = std::filesystem::exists(TexturePath);
    if (!bExists)
    {
        UE_LOG(Engine, Warning, "RmlUi texture not found: %s", Source.c_str());
        return 0;
    }

    ID3D11Resource* Resource = nullptr;
    FTexture StoredTexture = {};
    const HRESULT Result = DirectX::CreateWICTextureFromFile(Device, TexturePath.c_str(), &Resource, &StoredTexture.ShaderResourceView);
    if (FAILED(Result))
    {
        UE_LOG(Engine, Warning, "RmlUi failed to load texture: %s hr=0x%08X", FPaths::ToUtf8(TexturePath.wstring()).c_str(), static_cast<unsigned int>(Result));
        SafeRelease(Resource);
        SafeRelease(StoredTexture.ShaderResourceView);
        return 0;
    }

    ID3D11Texture2D* Texture = nullptr;
    if (Resource && SUCCEEDED(Resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&Texture))))
    {
        D3D11_TEXTURE2D_DESC Desc = {};
        Texture->GetDesc(&Desc);
        StoredTexture.Width = Desc.Width;
        StoredTexture.Height = Desc.Height;
        TextureDimensions = {static_cast<int>(Desc.Width), static_cast<int>(Desc.Height)};
    }

    SafeRelease(Texture);
    SafeRelease(Resource);

    const uint64 Handle = AllocateHandle();
    Textures.emplace(Handle, StoredTexture);
    return static_cast<Rml::TextureHandle>(Handle);
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
    Rml::SetFileInterface(&FileInterface);
    Rml::SetRenderInterface(&RenderInterface);
    Rml::SetFontEngineInterface(&FontEngine);

    if (!Rml::Initialise())
    {
        UE_LOG(Engine, Error, "RmlUi initialization failed.");
        RenderInterface.Shutdown();
        return false;
    }

    Context = Rml::CreateContext("Main", Rml::Vector2i(static_cast<int>(Width), static_cast<int>(Height)));
    if (!Context)
    {
        UE_LOG(Engine, Error, "Failed to create RmlUi context.");
        Rml::Shutdown();
        RenderInterface.Shutdown();
        return false;
    }

    bInitialized = true;
    LoadDefaultDocument();
    return true;
}

void FRmlUiManager::Shutdown()
{
    if (!bInitialized)
    {
        return;
    }

    Documents.clear();
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

void FRmlUiManager::SetViewportOffset(float X, float Y)
{
    ViewportOffsetX = X;
    ViewportOffsetY = Y;
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
        return Context->ProcessMouseMove(
            static_cast<int>(GET_X_LPARAM(LParam) - ViewportOffsetX),
            static_cast<int>(GET_Y_LPARAM(LParam) - ViewportOffsetY),
            Modifiers);
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
    else
    {
        UE_LOG(Engine, Warning, "RmlUi default document was not loaded: %s", FPaths::ToUtf8(DefaultDocumentPath).c_str());
    }
}

bool FRmlUiManager::LoadDocument(const FString& Name, const FString& DocumentPath)
{
    if (!Context || Name.empty() || DocumentPath.empty())
    {
        return false;
    }

    CloseDocument(Name);

    Rml::ElementDocument* Document = Context->LoadDocument(ResolveDocumentPath(DocumentPath));
    if (!Document)
    {
        UE_LOG(Engine, Error, "RmlUi failed to load document. name=%s path=%s", Name.c_str(), ResolveDocumentPath(DocumentPath).c_str());
        return false;
    }

    Documents[Name] = Document;
    Document->Show();
    UE_LOG(Engine, Info, "RmlUi loaded document. name=%s path=%s", Name.c_str(), ResolveDocumentPath(DocumentPath).c_str());
    return true;
}

bool FRmlUiManager::CloseDocument(const FString& Name)
{
    auto It = Documents.find(Name);
    if (It == Documents.end())
    {
        return false;
    }

    Rml::ElementDocument* Document = It->second;
    Documents.erase(It);
    if (Document)
    {
        Document->Close();
    }
    return true;
}

bool FRmlUiManager::ShowDocument(const FString& Name, bool bShow)
{
    Rml::ElementDocument* Document = FindDocument(Name);
    if (!Document)
    {
        return false;
    }

    if (bShow)
    {
        Document->Show();
    }
    else
    {
        Document->Hide();
    }
    return true;
}

bool FRmlUiManager::IsDocumentLoaded(const FString& Name) const
{
    return FindDocument(Name) != nullptr;
}

bool FRmlUiManager::SetDocumentPosition(const FString& Name, float X, float Y)
{
    Rml::ElementDocument* Document = FindDocument(Name);
    if (!Document)
    {
        return false;
    }

    Document->SetProperty("position", "absolute");
    Document->SetProperty("left", FormatPx(X));
    Document->SetProperty("top", FormatPx(Y));
    return true;
}

bool FRmlUiManager::SetDocumentSize(const FString& Name, float Width, float Height)
{
    Rml::ElementDocument* Document = FindDocument(Name);
    if (!Document)
    {
        return false;
    }

    Document->SetProperty("width", FormatPx(Width));
    Document->SetProperty("height", FormatPx(Height));
    return true;
}

bool FRmlUiManager::SetDocumentScale(const FString& Name, float Scale)
{
    Rml::ElementDocument* Document = FindDocument(Name);
    if (!Document)
    {
        return false;
    }

    Document->SetProperty("transform-origin", "0px 0px");
    Document->SetProperty("transform", "scale(" + FormatFloat(Scale) + ")");
    return true;
}

bool FRmlUiManager::SetDocumentZOrder(const FString& Name, int32 ZOrder)
{
    Rml::ElementDocument* Document = FindDocument(Name);
    if (!Document)
    {
        return false;
    }

    Document->SetProperty("z-index", std::to_string(ZOrder));
    return true;
}

bool FRmlUiManager::SetDocumentLayout(const FString& Name, float X, float Y, float Width, float Height, float Scale, int32 ZOrder)
{
    return SetDocumentPosition(Name, X, Y)
        && SetDocumentSize(Name, Width, Height)
        && SetDocumentScale(Name, Scale)
        && SetDocumentZOrder(Name, ZOrder);
}

bool FRmlUiManager::SetElementText(const FString& DocumentName, const FString& ElementId, const FString& Text)
{
    Rml::Element* Element = FindElement(DocumentName, ElementId);
    if (!Element)
    {
        return false;
    }

    Element->SetInnerRML("");
    const std::string TextTextureSource = CreateRmlTextBitmapSource(Text);
    if (TextTextureSource.empty())
    {
        return false;
    }

    Element->SetProperty("width", "360px");
    Element->SetProperty("height", "48px");
    Element->SetProperty("decorator", "image(" + TextTextureSource + ")");
    return true;
}

bool FRmlUiManager::SetElementClass(const FString& DocumentName, const FString& ElementId, const FString& ClassName, bool bEnabled)
{
    Rml::Element* Element = FindElement(DocumentName, ElementId);
    if (!Element)
    {
        return false;
    }

    Element->SetClass(ClassName, bEnabled);
    return true;
}

bool FRmlUiManager::SetElementProperty(const FString& DocumentName, const FString& ElementId, const FString& PropertyName, const FString& Value)
{
    Rml::Element* Element = FindElement(DocumentName, ElementId);
    if (!Element)
    {
        return false;
    }

    const bool bSucceeded = Element->SetProperty(PropertyName, Value);
    return bSucceeded;
}

bool FRmlUiManager::SetElementTexture(const FString& DocumentName, const FString& ElementId, const FString& TexturePath)
{
    Rml::ElementDocument* Document = FindDocument(DocumentName);
    if (!Document)
    {
        UE_LOG(Engine, Warning, "RmlUi SetElementTexture failed. document=%s element=%s source=%s", DocumentName.c_str(), ElementId.c_str(), TexturePath.c_str());
        return false;
    }

    Rml::Element* Element = Document->GetElementById(ElementId);
    if (!Element)
    {
        UE_LOG(Engine, Warning, "RmlUi SetElementTexture failed. document=%s element=%s source=%s", DocumentName.c_str(), ElementId.c_str(), TexturePath.c_str());
        return false;
    }

    const std::string Source = ResolveRmlTextureSource(Document->GetSourceURL(), TexturePath);
    Element->SetAttribute("src", Source);
    Element->SetProperty("decorator", "image(" + Source + ")");
    UE_LOG(Engine, Info, "RmlUi SetElementTexture. document=%s element=%s source=%s resolved=%s", DocumentName.c_str(), ElementId.c_str(), TexturePath.c_str(), Source.c_str());
    return true;
}

Rml::ElementDocument* FRmlUiManager::FindDocument(const FString& Name) const
{
    auto It = Documents.find(Name);
    return It != Documents.end() ? It->second : nullptr;
}

Rml::Element* FRmlUiManager::FindElement(const FString& DocumentName, const FString& ElementId) const
{
    Rml::ElementDocument* Document = FindDocument(DocumentName);
    return Document ? Document->GetElementById(ElementId) : nullptr;
}

Rml::String FRmlUiManager::ResolveDocumentPath(const FString& DocumentPath) const
{
    std::filesystem::path Path = FPaths::ToPath(DocumentPath);
    if (!Path.is_absolute())
    {
        Path = std::filesystem::path(FPaths::ContentDir()) / Path;
    }

    return FPaths::ToUtf8(Path.lexically_normal().wstring());
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
