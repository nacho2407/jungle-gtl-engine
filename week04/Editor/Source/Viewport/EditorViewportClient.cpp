#include "Viewport/EditorViewportClient.h"

void FEditorViewportClient::Create()
{
    // InputRouter 생성 및 필요한 InputContext 등록
    InputRouter = new Engine::ApplicationCore::FInputRouter();
    InputRouter->AddContext(&ViewportInputContext);

    NavigationController.SetCamera(&ViewportCamera);

    ViewportCamera.SetProjectionType(EViewportProjectionType::Perspective);
    ViewportCamera.SetFOV(90.0f);
    ViewportCamera.SetNearPlane(0.1f);
    ViewportCamera.SetFarPlane(2000.0f);
    ViewportCamera.SetLocation(FVector(0.0f, 3.0f, -8.0f));
    ViewportCamera.SetRotation(FRotator(0.0f, 0.0f, 0.0f));
}

void FEditorViewportClient::Release()
{
    if (InputRouter)
    {
        // 필요한 경우 InputRouter에서 등록된 Context 해제
        // 필요한 리소스 해제
        delete InputRouter;
        InputRouter = nullptr;
    }
}

void FEditorViewportClient::Tick(float DeltaTime, const Engine::ApplicationCore::FInputState& State)
{
    ViewportInputContext.SetDeltaTime(DeltaTime);

    if (InputRouter)
    {
        InputRouter->Tick(State);
    }

    NavigationController.Tick(DeltaTime);
}

void FEditorViewportClient::Draw()
{
    // 매 프레임마다 필요한 그리기 작업 수행
    // GizmoController.Draw();
    // SelectionController.Draw();
}

void FEditorViewportClient::HandleInputEvent(const Engine::ApplicationCore::FInputEvent& Event,
                                             const Engine::ApplicationCore::FInputState& State)
{
    if (InputRouter)
    {
        InputRouter->RouteEvent(Event, State);
    }
}

void FEditorViewportClient::BuildRenderData(FEditorRenderData& OutRenderData) const
{
    OutRenderData.bShowGrid = true;
    OutRenderData.bShowWorldAxes = true;
    OutRenderData.bShowGizmo = true;
    OutRenderData.bShowSelectionOutline = true;
    OutRenderData.bShowObjectLabels = true;

    OutRenderData.Gizmo.bVisible = true;
    OutRenderData.Gizmo.GizmoType = EGizmoType::Translation;
    OutRenderData.Gizmo.Highlight = EGizmoHighlight::None;
    OutRenderData.Gizmo.Transform = FMatrix::Identity;
}

void FEditorViewportClient::OnResize(uint32 Width, uint32 Height)
{
    ViewportCamera.OnResize(Width, Height);
}