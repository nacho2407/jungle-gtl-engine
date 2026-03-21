#pragma once

#include "Core/Containers/Array.h"
#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/Types/VertexTypes.h"

class FD3D11DynamicRHI;
class FSceneView;
struct FAABB;

class FD3D11LineBatchRenderer
{
  public:
    void Initialize(FD3D11DynamicRHI *InRHI) {}
    void Shutdown() {}

    void BeginFrame() {}
    void AddLine(const FVector &InStart, const FVector &InEnd, const FVector4 &InColor) {}
    void AddGrid(int32 InHalfLineCount, float InSpacing, const FVector4 &InColor) {}
    void AddWorldAxes(float InAxisLength) {}
    void AddAABB(const FAABB &InBounds, const FVector4 &InColor) {}
    void EndFrame(const FSceneView *InSceneView) {}

  private:
    void CreateShaders() {}
    void CreateInputLayout() {}
    void CreateConstantBuffer() {}
    void CreateDynamicVertexBuffer() {}
    void Flush(const FSceneView *InSceneView) {}

  private:
    FD3D11DynamicRHI *RHI = nullptr;

    TArray<FLineVertex> Vertices;

    TComPtr<ID3D11VertexShader> VertexShader;
    TComPtr<ID3D11PixelShader>  PixelShader;
    TComPtr<ID3D11InputLayout>  InputLayout;
    TComPtr<ID3D11Buffer>       ConstantBuffer;
    TComPtr<ID3D11Buffer>       DynamicVertexBuffer;
};