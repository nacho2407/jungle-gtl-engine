#pragma once

#include "Containers/Array.h"
#include "D3D11/D3D11Common.h"
#include "HAL/PlatformTypes.h"
#include "Renderer/Types/ViewMode.h"

class FD3D11DynamicRHI;
class FSceneView;
class UPrimitiveComponent;

struct FMeshDrawCommand
{
    const UPrimitiveComponent *Primitive = nullptr;
};

class FD3D11MeshBatchRenderer
{
  public:
    void Initialize(FD3D11DynamicRHI *InRHI) {}
    void Shutdown() {}

    void BeginFrame(EViewModeIndex InViewMode) {}
    void SubmitPrimitive(const UPrimitiveComponent *InPrimitive) {}
    void EndFrame(const FSceneView *InSceneView) {}

  private:
    void CreateShaders() {}
    void CreateInputLayouts() {}
    void CreateConstantBuffers() {}
    void CreateStates() {}

    void SortDrawCommands() {}
    void Flush(const FSceneView *InSceneView) {}

    void BindLitPipeline() {}
    void BindUnlitPipeline() {}
    void BindSolidRasterizer() {}
    void BindWireframeRasterizer() {}

    void DrawPrimitiveLit(const UPrimitiveComponent *InPrimitive, const FSceneView *InSceneView) {}
    void DrawPrimitiveUnlit(const UPrimitiveComponent *InPrimitive, const FSceneView *InSceneView)
    {
    }

  private:
    FD3D11DynamicRHI *RHI = nullptr;
    EViewModeIndex    ViewMode = EViewModeIndex::Lit;

    TArray<FMeshDrawCommand> DrawCommands;

    TComPtr<ID3D11VertexShader> LitVertexShader;
    TComPtr<ID3D11PixelShader>  LitPixelShader;
    TComPtr<ID3D11VertexShader> UnlitVertexShader;
    TComPtr<ID3D11PixelShader>  UnlitPixelShader;
    TComPtr<ID3D11InputLayout>  InputLayout;

    TComPtr<ID3D11Buffer> LitConstantBuffer;
    TComPtr<ID3D11Buffer> UnlitConstantBuffer;

    TComPtr<ID3D11RasterizerState>   SolidRasterizerState;
    TComPtr<ID3D11RasterizerState>   WireframeRasterizerState;
    TComPtr<ID3D11DepthStencilState> DepthStencilState;
};