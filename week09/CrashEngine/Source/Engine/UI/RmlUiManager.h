#pragma once

#include "Core/CoreTypes.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"

#ifdef GetFirstChild
#undef GetFirstChild
#endif
#ifdef GetNextSibling
#undef GetNextSibling
#endif

#include <RmlUi/Core.h>
#include <RmlUi/Core/FileInterface.h>

#include <d3d11.h>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

class FWindowsWindow;
class FRenderer;
struct HWND__;

class FRmlUiSystemInterface final : public Rml::SystemInterface
{
public:
    double GetElapsedTime() override;
    bool LogMessage(Rml::Log::Type Type, const Rml::String& Message) override;
};

class FRmlUiNullFontEngine final : public Rml::FontEngineInterface
{
public:
    bool LoadFontFace(const Rml::String& FilePath, int FaceIndex, bool FallbackFace, Rml::Style::FontWeight Weight) override;
    bool LoadFontFace(const Rml::String& FilePath, int FaceIndex, const Rml::String& Family, Rml::Style::FontStyle Style, Rml::Style::FontWeight Weight, bool FallbackFace) override;
    bool LoadFontFace(Rml::Span<const Rml::byte> Data, int FaceIndex, const Rml::String& Family, Rml::Style::FontStyle Style, Rml::Style::FontWeight Weight, bool FallbackFace) override;
};

class FRmlUiFileInterface final : public Rml::FileInterface
{
public:
    Rml::FileHandle Open(const Rml::String& Path) override;
    void Close(Rml::FileHandle File) override;
    size_t Read(void* Buffer, size_t Size, Rml::FileHandle File) override;
    bool Seek(Rml::FileHandle File, long Offset, int Origin) override;
    size_t Tell(Rml::FileHandle File) override;
};

class FRmlUiD3D11RenderInterface final : public Rml::RenderInterface
{
public:
    bool Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InContext);
    void Shutdown();
    void SetViewportSize(uint32 Width, uint32 Height);

    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> Vertices, Rml::Span<const int> Indices) override;
    void RenderGeometry(Rml::CompiledGeometryHandle Geometry, Rml::Vector2f Translation, Rml::TextureHandle Texture) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle Geometry) override;

    Rml::TextureHandle LoadTexture(Rml::Vector2i& TextureDimensions, const Rml::String& Source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> Source, Rml::Vector2i SourceDimensions) override;
    void ReleaseTexture(Rml::TextureHandle Texture) override;

    void EnableScissorRegion(bool Enable) override;
    void SetScissorRegion(Rml::Rectanglei Region) override;
    void SetTransform(const Rml::Matrix4f* Transform) override;

private:
    struct FCompiledGeometry
    {
        ID3D11Buffer* VertexBuffer = nullptr;
        ID3D11Buffer* IndexBuffer = nullptr;
        uint32 IndexCount = 0;
    };

    struct FTexture
    {
        ID3D11ShaderResourceView* ShaderResourceView = nullptr;
        uint32 Width = 0;
        uint32 Height = 0;
    };

    struct FVertex
    {
        float Position[2];
        uint8 Color[4];
        float TexCoord[2];
    };

    struct FConstants
    {
        float Transform[16];
    };

private:
    bool CreateDeviceResources();
    void ReleaseDeviceResources();
    void UpdateConstants(Rml::Vector2f Translation);
    uint64 AllocateHandle();

private:
    ID3D11Device* Device = nullptr;
    ID3D11DeviceContext* Context = nullptr;

    ID3D11VertexShader* VertexShader = nullptr;
    ID3D11PixelShader* PixelShader = nullptr;
    ID3D11InputLayout* InputLayout = nullptr;
    ID3D11Buffer* ConstantBuffer = nullptr;
    ID3D11SamplerState* SamplerState = nullptr;
    ID3D11ShaderResourceView* WhiteTexture = nullptr;
    ID3D11BlendState* BlendState = nullptr;
    ID3D11RasterizerState* RasterizerState = nullptr;
    ID3D11RasterizerState* ScissorRasterizerState = nullptr;
    ID3D11DepthStencilState* DepthStencilState = nullptr;

    std::unordered_map<uint64, FCompiledGeometry> Geometries;
    std::unordered_map<uint64, FTexture> Textures;

    Rml::Matrix4f CurrentTransform = Rml::Matrix4f::Identity();
    bool bHasTransform = false;
    bool bScissorEnabled = false;
    uint32 ViewportWidth = 1;
    uint32 ViewportHeight = 1;
    uint64 NextHandle = 1;
};

class FRmlUiManager
{
public:
    bool Initialize(FWindowsWindow* Window, FRenderer& Renderer);
    void Shutdown();

    void Update();
    void Render();
    void OnWindowResized(uint32 Width, uint32 Height);
    void SetViewportOffset(float X, float Y);
    bool HandleWindowMessage(HWND__* WindowHandle, unsigned int Message, WPARAM WParam, LPARAM LParam);

    Rml::Context* GetContext() const { return Context; }
    bool IsInitialized() const { return bInitialized; }

    bool LoadDocument(const FString& Name, const FString& DocumentPath);
    bool CloseDocument(const FString& Name);
    bool ShowDocument(const FString& Name, bool bShow = true);
    bool IsDocumentLoaded(const FString& Name) const;

    bool SetDocumentPosition(const FString& Name, float X, float Y);
    bool SetDocumentSize(const FString& Name, float Width, float Height);
    bool SetDocumentScale(const FString& Name, float Scale);
    bool SetDocumentZOrder(const FString& Name, int32 ZOrder);
    bool SetDocumentLayout(const FString& Name, float X, float Y, float Width, float Height, float Scale, int32 ZOrder);

    bool SetElementText(const FString& DocumentName, const FString& ElementId, const FString& Text, int32 FontSize = 32);
    bool SetElementTextColor(const FString& DocumentName, const FString& ElementId, const FString& Text, int32 R, int32 G, int32 B, int32 FontSize = 32);
    bool SetElementTextStyle(const FString& DocumentName, const FString& ElementId, const FString& Text, int32 R, int32 G, int32 B, int32 FontSize, bool bBold, int32 CenterWidth);
    bool SetElementClass(const FString& DocumentName, const FString& ElementId, const FString& ClassName, bool bEnabled);
    bool SetElementProperty(const FString& DocumentName, const FString& ElementId, const FString& PropertyName, const FString& Value);
    bool SetElementTexture(const FString& DocumentName, const FString& ElementId, const FString& TexturePath);

private:
    void LoadDefaultDocument();
    Rml::ElementDocument* FindDocument(const FString& Name) const;
    Rml::Element* FindElement(const FString& DocumentName, const FString& ElementId) const;
    Rml::String ResolveDocumentPath(const FString& DocumentPath) const;
    int GetKeyModifierState() const;
    Rml::Input::KeyIdentifier TranslateKey(WPARAM VirtualKey, LPARAM KeyData) const;

private:
    FRmlUiSystemInterface SystemInterface;
    FRmlUiFileInterface FileInterface;
    FRmlUiNullFontEngine FontEngine;
    FRmlUiD3D11RenderInterface RenderInterface;
    Rml::Context* Context = nullptr;
    std::unordered_map<std::string, Rml::ElementDocument*> Documents;
    float ViewportOffsetX = 0.0f;
    float ViewportOffsetY = 0.0f;
    bool bInitialized = false;
};
