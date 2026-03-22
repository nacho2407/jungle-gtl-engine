#include "Editor.h"
#include "Viewport/EditorViewportClient.h"

void FEditor::Create()
{
    //  TODO : Viewport Client
    ViewportClient.Create();

    //  TODO : Panel UI

    //  TODO : Gizmo
}

void FEditor::Release()
{
    //  TODO : Call Release Functions
    ViewportClient.Release();
}

void FEditor::Initialize()
{
    CurScene = new FScene();
    //  TODO : Scene의 Begin Play 호출
}

void FEditor::Tick(float DeltaTime, Engine::ApplicationCore::FInputSystem* InputSystem)
{
    Engine::ApplicationCore::FInputEvent Event;

    while (InputSystem->PollEvent(Event))
    {
        ViewportClient.HandleInputEvent(Event, InputSystem->GetInputState());
    }

    ViewportClient.Tick(DeltaTime, InputSystem->GetInputState());

    BuildRenderData();
}

void FEditor::OnWindowResized(float Width, float Height)
{
    if (Width <= 0 || Height <= 0)
    {
        return;
    }

    WindowHeight = Height;
    WindowWidth = Width;
    //  TODO : Setting Panel Size
}

void FEditor::CreateNewScene()
{
    ClearScene();
    //  TODO : 새로운 Scene으로 교체, Panel 초기화,
}

void FEditor::ClearScene()
{
    //  TODO : Scene에 대한 모든 정보 제거
}

void FEditor::BuildSceneView()
{
    SceneView.SetViewMatrix(ViewportClient.GetCamera().GetViewMatrix());
    SceneView.SetProjectionMatrix(ViewportClient.GetCamera().GetProjectionMatrix());
    SceneView.SetViewLocation(ViewportClient.GetCamera().GetLocation());

    FViewportRect ViewRect;
    ViewRect.X = 0;
    ViewRect.Y = 0;
    ViewRect.Width = static_cast<int32>(WindowWidth);
    ViewRect.Height = static_cast<int32>(WindowHeight);

    SceneView.SetViewRect(ViewRect);
    SceneView.SetClipPlanes(ViewportClient.GetCamera().GetNearPlane(),
                            ViewportClient.GetCamera().GetFarPlane());
}

void FEditor::BuildRenderData()
{
    EditorRenderData = FEditorRenderData{};
    SceneRenderData = FSceneRenderData{};

    BuildSceneView();

    EditorRenderData.SceneView = &SceneView;
    SceneRenderData.SceneView = &SceneView;

    ViewportClient.BuildRenderData(EditorRenderData);

    if (CurScene != nullptr)
    {
        CurScene->BuildRenderData(SceneRenderData);
    }
}
