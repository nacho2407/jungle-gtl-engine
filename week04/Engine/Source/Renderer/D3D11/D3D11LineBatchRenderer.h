#pragma once

#include "Core/Containers/Array.h"
#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/Types/VertexTypes.h"
#include "Renderer/D3D11/D3D11Common.h"
#include "Core/Geometry/Primitives/AABB.h"

class FD3D11DynamicRHI;
class FSceneView;
struct FAABB;

class FD3D11LineBatchRenderer
{
  public:
    static constexpr const wchar_t* DefaultShaderPath = L"Engine/Resources/Shader/ShaderLine.hlsl";
    static constexpr uint32         DefaultMaxLineCount = 8192;

  public:
    bool Initialize(FD3D11DynamicRHI* InRHI);
    void Shutdown();

    void BeginFrame(const FSceneView* InSceneView);
    void AddLine(const FVector& InStart, const FVector& InEnd, const FColor& InColor);
    void AddGrid(int32 InHalfLineCount, float InSpacing, const FColor& InColor);
    void AddWorldAxes(float InAxisLength);
    void AddAABB(const Geometry::FAABB& InBounds, const FColor& InColor);
    void EndFrame();

  private:
    bool CreateShaders();
    bool CreateConstantBuffer();
    bool CreateDynamicVertexBuffer(uint32 InMaxVertexCount);
    void Flush();

  private:
    FD3D11DynamicRHI* RHI = nullptr;
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