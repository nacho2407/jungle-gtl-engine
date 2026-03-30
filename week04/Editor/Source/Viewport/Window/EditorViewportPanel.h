#pragma once

#include "Viewport/EditorViewport.h"
#include "Viewport/EditorViewportClient.h"
#include "Core/Runtime/Slate/Window/SWindow.h"
#include "Renderer/SceneView.h"

class FEditorViewportPanel : public SWindow
{
  public:   
    FEditorViewportPanel() = default;
    FEditorViewport*       Viewport       = nullptr; // 렌더 타겟 텍스처 관리 (출력)
    FEditorViewportClient* ViewportClient = nullptr; // 카메라 및 입력 로직 관리 (입력/제어)

    FEditorRenderData EditorRenderData;

    void PrepareRender();
    void BuildRenderData();

    // Per-panel scene view built from this panel's camera each frame.
    // Stored here so the pointer passed to FSceneRenderData stays valid
    // for the duration of the Renderer->Render() call.
    FSceneView SceneView;
};
