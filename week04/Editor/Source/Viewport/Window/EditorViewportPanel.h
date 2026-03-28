#pragma once

#include "Viewport/EditorViewport.h"
#include "Viewport/EditorViewportClient.h"
#include "Renderer/SceneView.h"
#include "Renderer/SceneRenderData.h"

struct FEditorViewportPanel
{
    FEditorViewport*       Viewport       = nullptr; // 렌더 타겟 텍스처 관리 (출력)
    FEditorViewportClient* ViewportClient = nullptr; // 카메라 및 입력 로직 관리 (입력/제어)
    FScene*                Scene          = nullptr;

    FSceneRenderData  SceneRenderData;
    FEditorRenderData EditorRenderData;

    void PrepareRender();
    void BuildRenderData();

    // Per-panel scene view built from this panel's camera each frame.
    // Stored here so the pointer passed to FSceneRenderData stays valid
    // for the duration of the Renderer->Render() call.
    FSceneView SceneView;

    // 이 패널이 화면 어디에 그려지고 있는지 UI 좌표
    float PosX = 0.f, PosY = 0.f, Width = 0.f, Height = 0.f;

    bool bIsActive = false;
};
