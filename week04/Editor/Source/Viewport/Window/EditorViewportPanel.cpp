#include "EditorViewportPanel.h"
#include "Engine/Scene.h"

void FEditorViewportPanel::PrepareRender() {
    FViewportCamera& Cam = ViewportClient->GetCamera();

    SceneView.SetViewMatrix(Cam.GetViewMatrix());
    SceneView.SetProjectionMatrix(Cam.GetProjectionMatrix());
    SceneView.SetViewLocation(Cam.GetLocation());
    SceneView.SetClipPlanes(Cam.GetNearPlane(), Cam.GetFarPlane());

    FViewportRect VR;
    VR.X = static_cast<int32>(PosX);
    VR.Y = static_cast<int32>(PosY);
    VR.Width = static_cast<int32>(Width);
    VR.Height = static_cast<int32>(Height);
    SceneView.SetViewRect(VR);
}

void FEditorViewportPanel::BuildRenderData() 
{
	SceneRenderData = {};
    EditorRenderData = {};
    EditorRenderData.SceneView = &SceneView;
    SceneRenderData.SceneView = &SceneView;
    SceneRenderData.ViewMode = ViewportClient->GetRenderSetting().GetViewMode();

    const EEditorShowFlags EditorShowFlags =
        ViewportClient->GetRenderSetting().BuildEditorShowFlags(true);
    const ESceneShowFlags SceneShowFlags =
        ViewportClient->GetRenderSetting().BuildSceneShowFlags();

    ViewportClient->BuildRenderData(EditorRenderData, EditorShowFlags);

    if (Scene)
    {
        Scene->BuildRenderData(SceneRenderData, SceneShowFlags);
    }
}