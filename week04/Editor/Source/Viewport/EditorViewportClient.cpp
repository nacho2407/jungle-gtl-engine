#include "Viewport/EditorViewportClient.h"
#include "ApplicationCore/Input/InputRouter.h"

void FEditorViewportClient::Create()
{
    // InputRouter 생성 및 필요한 InputContext 등록
    InputRouter = new Engine::ApplicationCore::FInputRouter();
    InputRouter->AddContext(&ViewportInputContext);

    NavigationController.SetCamera(&ViewportCamera);

    ViewportCamera.SetProjectionType(EViewportProjectionType::Perspective);
    ViewportCamera.SetFOV(3.141592f * 0.5f); // TODO: 라디안으로 고치기
    ViewportCamera.SetNearPlane(0.1f);
    ViewportCamera.SetFarPlane(2000.0f);
    ViewportCamera.SetLocation(FVector(-10.0f, 5.0f, 50.0f));
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
    OutRenderData.Gizmo.GizmoType = EGizmoType::Translation;
    OutRenderData.Gizmo.Highlight = EGizmoHighlight::None;
    OutRenderData.Gizmo.Transform = FMatrix::Identity;
}

void FEditorViewportClient::OnResize(uint32 Width, uint32 Height)
{
    ViewportCamera.OnResize(Width, Height);
}