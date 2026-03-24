#pragma once

#include "Core/Containers/Array.h"
#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/Types/VertexTypes.h"

class FD3D11RHI;
class FSceneView;

class FD3D11LineBatchRenderer
{
  public:
    static constexpr const wchar_t* DefaultShaderPath =
        L"../Engine/Resources/Shader/ShaderLine.hlsl";
    static constexpr uint32 DefaultMaxLineCount = 8192;

  public:
    bool Initialize(FD3D11RHI* InRHI);
    void Shutdown();

    void BeginFrame(const FSceneView* InSceneView);
    void AddLine(const FVector& InStart, const FVector& InEnd, const FColor& InColor);
    void EndFrame();
    void Flush();

  private:
    bool CreateShaders();
    bool CreateConstantBuffer();
    bool CreateDynamicVertexBuffer(uint32 InMaxVertexCount);

  private:
    FD3D11RHI* RHI = nullptr;
    const FSceneView* CurrentSceneView = nullptr;

    TArray<FLineVertex> Vertices;

    uint32 MaxLineCount = DefaultMaxLineCount;
    uint32 MaxVertexCount = DefaultMaxLineCount * 2;

    TComPtr<ID3D11VertexShader> VertexShader;
    TComPtr<ID3D11PixelShader>  PixelShader;
    TComPtr<ID3D11InputLayout>  InputLayout;
    TComPtr<ID3D11Buffer>       ConstantBuffer;
    TComPtr<ID3D11Buffer>       DynamicVertexBuffer;
};