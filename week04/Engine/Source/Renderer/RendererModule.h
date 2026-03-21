#pragma once

#include <Windows.h>

#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/D3D11/D3D11DynamicRHI.h"
#include "Renderer/D3D11/D3D11FontBatchRenderer.h"
#include "Renderer/D3D11/D3D11LineBatchRenderer.h"
#include "Renderer/D3D11/D3D11MeshBatchRenderer.h"
#include "Renderer/D3D11/D3D11GizmoRenderer.h"
#include "Renderer/D3D11/D3D11PickingPass.h"
#include "Renderer/D3D11/D3D11SpriteBatchRenderer.h"
#include "Renderer/EditorRenderData.h"
#include "Renderer/SceneRenderData.h"

// TODO: Font, Sprite, Pick 렌더러 사용

class FRendererModule
{
  public:
    void StartupModule(HWND hWnd);
    void ShutdownModule();

    void BeginFrame();
    void EndFrame();

    void OnWindowResized(int32 InWidth, int32 InHeight);

    void RenderFrame(const FEditorRenderData& InEditorRenderData,
                     const FSceneRenderData&  InSceneRenderData);

    bool TryConsumePickResult(uint32& OutPickId);
    void RequestPick(const FEditorRenderData& InEditorRenderData, int32 MouseX, int32 MouseY);

    FD3D11DynamicRHI& GetRHI() { return RHI; }

  private:
    FD3D11DynamicRHI RHI;

    FD3D11MeshBatchRenderer   MeshRenderer;
    FD3D11LineBatchRenderer   LineRenderer;
    FD3D11GizmoRenderer       GizmoRenderer;
    FD3D11FontBatchRenderer   FontRenderer;
    FD3D11SpriteBatchRenderer SpriteRenderer;
    FD3D11PickingPass         PickingPass;

    TComPtr<ID3D11Debug> DebugDevice;
};