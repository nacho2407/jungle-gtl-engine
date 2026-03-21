#pragma once

#include <Windows.h>

#include "D3D11/D3D11Common.h"
#include "D3D11/D3D11DynamicRHI.h"
#include "D3D11/D3D11FontBatchRenderer.h"
#include "D3D11/D3D11LineBatchRenderer.h"
#include "D3D11/D3D11MeshBatchRenderer.h"
#include "D3D11/D3D11PickingPass.h"
#include "D3D11/D3D11SpriteBatchRenderer.h"
#include "Types/EditorRenderData.h"
#include "Types/SceneRenderData.h"

class FScene;

class FRendererModule
{
  public:
    void StartupModule(HWND hWnd) {}
    void ShutdownModule() {}

    void BeginFrame() {}
    void EndFrame() {}

    void OnWindowResized(int32 InWidth, int32 InHeight) {}

    void    SetScene(FScene *InScene) {}
    FScene *GetScene() const { return nullptr; }

    void RenderFrame(const FEditorRenderData &InEditorRenderData,
                     const FSceneRenderData  &InMeshRenderData)
    {
    }

    void RenderWorld(const FSceneRenderData &InMeshRenderData) {}
    void RenderEditorOverlays(const FEditorRenderData &InEditorRenderData) {}

    void SubmitWorldMeshes(const FSceneRenderData &InMeshRenderData) {}
    void SubmitWorldSprites(const FSceneRenderData &InMeshRenderData) {}

    void RenderGrid(const FEditorRenderData &InEditorRenderData) {}
    void RenderWorldAxes(const FEditorRenderData &InEditorRenderData) {}
    void RenderTransformGizmo(const FEditorRenderData &InEditorRenderData) {}
    void RenderSelectionBounds(const FEditorRenderData &InEditorRenderData) {}
    void RenderObjectLabels(const FEditorRenderData &InEditorRenderData) {}

    bool TryConsumePickResult(uint32 &OutPickId) {}
    void RequestPick(const FEditorRenderData &InEditorRenderData, int32 MouseX, int32 MouseY) {}

    FD3D11DynamicRHI &GetRHI() { return RHI; }

    FD3D11MeshBatchRenderer   &GetMeshRenderer() { return MeshRenderer; }
    FD3D11LineBatchRenderer   &GetLineRenderer() { return LineRenderer; }
    FD3D11FontBatchRenderer   &GetFontRenderer() { return FontRenderer; }
    FD3D11SpriteBatchRenderer &GetSpriteRenderer() { return SpriteRenderer; }

  private:
    FD3D11DynamicRHI RHI;

    FD3D11MeshBatchRenderer   MeshRenderer;
    FD3D11LineBatchRenderer   LineRenderer;
    FD3D11FontBatchRenderer   FontRenderer;
    FD3D11SpriteBatchRenderer SpriteRenderer;
    FD3D11PickingPass         PickingPass;

    FScene *Scene = nullptr;

    TComPtr<ID3D11Debug> DebugDevice;
};