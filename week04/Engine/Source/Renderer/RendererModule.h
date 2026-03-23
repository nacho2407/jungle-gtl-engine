#pragma once

#include <Windows.h>

#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/D3D11/D3D11DynamicRHI.h"
#include "Renderer/D3D11/D3D11FontBatchRenderer.h"
#include "Renderer/D3D11/D3D11LineBatchRenderer.h"
#include "Renderer/D3D11/D3D11MeshBatchRenderer.h"
#include "Renderer/D3D11/D3D11ObjectIdRenderer.h"
#include "Renderer/D3D11/D3D11SpriteBatchRenderer.h"
#include "Renderer/Draw/GizmoDrawer.h"
#include "Renderer/Draw/PrimitiveDrawer.h"
#include "Renderer/Draw/WorldAxesDrawer.h"
#include "Renderer/Draw/WorldGridDrawer.h"
#include "Renderer/EditorRenderData.h"
#include "Renderer/SceneRenderData.h"
#include "Renderer/Types/PickResult.h"

class ENGINE_API FRendererModule
{
  public:
    bool StartupModule(HWND hWnd);
    void ShutdownModule();

    void BeginFrame();
    void EndFrame();

    void OnWindowResized(int32 InWidth, int32 InHeight);

    void Render(const FEditorRenderData& InEditorRenderData,
                const FSceneRenderData&  InSceneRenderData);

    bool Pick(const FEditorRenderData& InEditorRenderData, int32 MouseX, int32 MouseY,
              FPickResult& OutResult);

    FD3D11DynamicRHI& GetRHI() { return RHI; }

    void SetVSyncEnabled(bool bEnabled);
    bool IsVSyncEnabled() const;

  private:
    FD3D11DynamicRHI RHI;

    FD3D11MeshBatchRenderer   MeshRenderer;
    FD3D11LineBatchRenderer   LineRenderer;
    FD3D11FontBatchRenderer   FontRenderer;
    FD3D11SpriteBatchRenderer SpriteRenderer;
    FD3D11ObjectIdRenderer    ObjectIdRenderer;

    FPrimitiveDrawer PrimitiveDrawer;
    FWorldGridDrawer WorldGridDrawer;
    FWorldAxesDrawer WorldAxesDrawer;
    FGizmoDrawer     GizmoDrawer;

    FEditorRenderData CachedEditorRenderData;
    FSceneRenderData  CachedSceneRenderData;

    TComPtr<ID3D11Debug> DebugDevice;

    bool PickRaw(const FEditorRenderData& InEditorRenderData, int32 MouseX, int32 MouseY,
                 uint32& OutPickId);
};