#pragma once

#include <Windows.h>

#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/D3D11/D3D11RHI.h"
#include "Renderer/D3D11/D3D11TextBatchRenderer.h"
#include "Renderer/D3D11/D3D11LineBatchRenderer.h"
#include "Renderer/D3D11/D3D11OverlayMeshRenderer.h"
#include "Renderer/D3D11/D3D11SceneMeshRenderer.h"
#include "Renderer/D3D11/D3D11OutlineRenderer.h"
#include "Renderer/D3D11/D3D11ObjectIdRenderer.h"
#include "Renderer/D3D11/D3D11SpriteBatchRenderer.h"
#include "Renderer/Submitter/AABBSubmitter.h"
#include "Renderer/Submitter/GizmoSubmitter.h"
#include "Renderer/Submitter/PrimitiveSubmitter.h"
#include "Renderer/Submitter/SpriteSubmitter.h"
#include "Renderer/Submitter/TextSubmitter.h"
#include "Renderer/Submitter/WorldAxesSubmitter.h"
#include "Renderer/Submitter/WorldGridSubmitter.h"
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

    FD3D11RHI& GetRHI() { return RHI; }

    void SetVSyncEnabled(bool bEnabled);
    bool IsVSyncEnabled() const;

  private:
    FD3D11RHI RHI;

    FD3D11SceneMeshRenderer   SceneMeshRenderer;
    FD3D11OverlayMeshRenderer OverlayMeshRenderer;
    FD3D11OutlineRenderer     OutlineRenderer;
    FD3D11LineBatchRenderer   LineRenderer;
    FD3D11TextBatchRenderer   TextRenderer;
    FD3D11SpriteBatchRenderer SpriteRenderer;
    FD3D11ObjectIdRenderer    ObjectIdRenderer;

    FPrimitiveSubmitter PrimitiveSubmitter;
    FSpriteSubmitter    SpriteSubmitter;
    FTextSubmitter      TextSubmitter;
    FAABBSubmitter      AABBSubmitter;
    FWorldGridSubmitter WorldGridSubmitter;
    FWorldAxesSubmitter WorldAxesSubmitter;
    FGizmoSubmitter    GizmoSubmitter;

    TComPtr<ID3D11Debug> DebugDevice;

    void RenderWorldPass(const FEditorRenderData& InEditorRenderData,
                         const FSceneRenderData&  InSceneRenderData);
    void RenderOverlayPass(const FEditorRenderData& InEditorRenderData,
                           const FSceneRenderData&  InSceneRenderData);

    bool PickRaw(const FEditorRenderData& InEditorRenderData, int32 MouseX, int32 MouseY,
                 uint32& OutPickId);
};
