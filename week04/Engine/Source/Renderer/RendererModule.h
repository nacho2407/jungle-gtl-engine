#pragma once

#include <Windows.h>

#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/D3D11/D3D11DynamicRHI.h"
#include "Renderer/D3D11/D3D11FontBatchRenderer.h"
#include "Renderer/D3D11/D3D11LineBatchRenderer.h"
#include "Renderer/D3D11/D3D11MeshBatchRenderer.h"
#include "Renderer/D3D11/D3D11PickingPass.h"
#include "Renderer/D3D11/D3D11SpriteBatchRenderer.h"
#include "Renderer/Draw/GizmoDrawer.h"
#include "Renderer/Draw/PrimitiveDrawer.h"
#include "Renderer/Draw/WorldAxesDrawer.h"
#include "Renderer/Draw/WorldGridDrawer.h"
#include "Renderer/EditorRenderData.h"
#include "Renderer/SceneRenderData.h"

// TODO: Font, Sprite, Pick 렌더러 사용

class ENGINE_API FRendererModule
{
  public:
    bool StartupModule(HWND hWnd);
    void ShutdownModule();

    void BeginFrame();
    void EndFrame();

    // swap chain과 back buffer를 현재 창 크기에 다시 맞춥니다.
    void OnWindowResized(int32 InWidth, int32 InHeight);
    // live resize 중에는 VSync를 잠시 끄고, 종료 후 원래 상태로 복구할 수 있게 노출한 얇은 래퍼입니다.
    void SetVSyncEnabled(bool bInVSyncEnabled);
    bool IsVSyncEnabled() const;

    void Render(const FEditorRenderData& InEditorRenderData,
                const FSceneRenderData&  InSceneRenderData);

    bool TryConsumePickResult(uint32& OutPickId);
    void RequestPick(const FEditorRenderData& InEditorRenderData, int32 MouseX, int32 MouseY);

    FD3D11DynamicRHI& GetRHI() { return RHI; }

  private:
    FD3D11DynamicRHI RHI;

    FD3D11MeshBatchRenderer   MeshRenderer;
    FD3D11LineBatchRenderer   LineRenderer;
    FD3D11FontBatchRenderer   FontRenderer;
    FD3D11SpriteBatchRenderer SpriteRenderer;
    FD3D11PickingPass         PickingPass;

    FPrimitiveDrawer PrimitiveDrawer;
    FWorldGridDrawer WorldGridDrawer;
    FWorldAxesDrawer WorldAxesDrawer;
    FGizmoDrawer     GizmoDrawer;

    TComPtr<ID3D11Debug> DebugDevice;
};
