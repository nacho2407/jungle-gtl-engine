#pragma once

#include "Core/Containers/String.h"
#include "Render/Common/RenderTypes.h"

#include "RmlUi/Core/RenderInterface.h"

#include <unordered_map>

class FRmlUiRenderInterfaceD3D11 final : public Rml::RenderInterface
{
public:
    bool Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InContext);
    void Shutdown();

    void BeginFrame(
        const Rml::Vector2f& InViewportMin,
        const Rml::Vector2f& InViewportSize,
        const Rml::Vector2f& InRenderScale = Rml::Vector2f(1.0f, 1.0f));

    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> Vertices, Rml::Span<const int> Indices) override;
    void RenderGeometry(Rml::CompiledGeometryHandle Geometry, Rml::Vector2f Translation, Rml::TextureHandle Texture) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle Geometry) override;

    Rml::TextureHandle LoadTexture(Rml::Vector2i& TextureDimensions, const Rml::String& Source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> Source, Rml::Vector2i SourceDimensions) override;
    void ReleaseTexture(Rml::TextureHandle Texture) override;

    void EnableScissorRegion(bool bEnable) override;
    void SetScissorRegion(Rml::Rectanglei Region) override;
    void SetTransform(const Rml::Matrix4f* Transform) override;

private:
    struct FCompiledGeometry
    {
        TComPtr<ID3D11Buffer> VertexBuffer;
        TComPtr<ID3D11Buffer> IndexBuffer;
        uint32 IndexCount = 0;
    };

    struct FGeneratedTexture
    {
        TComPtr<ID3D11Texture2D> Texture;
        TComPtr<ID3D11ShaderResourceView> SRV;
    };

    struct FFrameConstants
    {
        float ViewportSize[2] = { 1.0f, 1.0f };
        float Translation[2] = { 0.0f, 0.0f };
        float RenderScale[2] = { 1.0f, 1.0f };
        uint32 bUseTexture = 0;
        float Padding = 0.0f;
        float Transform[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
    };

    bool CreateShaders();
    bool CreateStates();
    void BindPipeline(bool bUseTexture);
    void ApplyScissorState();
    Rml::TextureHandle MakeGeneratedTextureHandle(ID3D11ShaderResourceView* SRV);

private:
    TComPtr<ID3D11Device> Device;
    TComPtr<ID3D11DeviceContext> Context;

    TComPtr<ID3D11VertexShader> VertexShader;
    TComPtr<ID3D11PixelShader> PixelShader;
    TComPtr<ID3D11InputLayout> InputLayout;
    TComPtr<ID3D11Buffer> ConstantBuffer;
    TComPtr<ID3D11SamplerState> SamplerState;
    TComPtr<ID3D11BlendState> BlendState;
    TComPtr<ID3D11DepthStencilState> DepthStencilState;
    TComPtr<ID3D11RasterizerState> RasterizerState;
    TComPtr<ID3D11RasterizerState> ScissorRasterizerState;

    Rml::Vector2f ViewportMin = Rml::Vector2f(0.0f, 0.0f);
    Rml::Vector2f ViewportSize = Rml::Vector2f(1.0f, 1.0f);
    Rml::Vector2f RenderScale = Rml::Vector2f(1.0f, 1.0f);
    Rml::Matrix4f CurrentTransform = Rml::Matrix4f::Identity();
    bool bScissorEnabled = false;
    Rml::Rectanglei CurrentScissorRegion;

    std::unordered_map<Rml::CompiledGeometryHandle, FCompiledGeometry> GeometryByHandle;
    std::unordered_map<Rml::TextureHandle, FGeneratedTexture> GeneratedTextures;
    Rml::CompiledGeometryHandle NextGeometryHandle = 1;
    Rml::TextureHandle NextGeneratedTextureHandle = 1;
};
