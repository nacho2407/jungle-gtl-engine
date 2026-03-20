#pragma once

#include "D3D11/D3D11Common.h"
#include "Math/Matrix.h"

class FD3D11DynamicRHI;
class FScene;
class FSceneView;
struct FEditorGizmoState;

class FD3D11PickingPass
{
  public:
    void Initialize(FD3D11DynamicRHI *InRHI) {}
    void Shutdown() {}

    void OnWindowResized(int32 InWidth, int32 InHeight) {}

    void RequestPick(FScene *InScene, const FSceneView *InSceneView, int32 MouseX, int32 MouseY,
                     const FEditorGizmoState &InGizmoState) {}

    bool TryConsumePickResult(uint32 &OutPickId) {}

  private:
    void CreateShaders() {}
    void CreateConstantBuffer() {}
    void CreatePickingTargets() {}
    void ReleasePickingTargets() {}

  private:
    FD3D11DynamicRHI *RHI = nullptr;

    int32 Width = 0;
    int32 Height = 0;

    TComPtr<ID3D11VertexShader> VertexShader;
    TComPtr<ID3D11PixelShader>  PixelShader;
    TComPtr<ID3D11InputLayout>  InputLayout;
    TComPtr<ID3D11Buffer>       ConstantBuffer;

    TComPtr<ID3D11Texture2D>        PickingTexture;
    TComPtr<ID3D11RenderTargetView> PickingRTV;
    TComPtr<ID3D11Texture2D>        PickingStagingTexture;

    bool   bHasPendingResult = false;
    uint32 PendingPickId = 0;
};