#include "Viewport/EditorViewportClient.h"

void FEditorViewportClient::Create()
{
    // InputRouter 생성 및 필요한 InputContext 등록
    InputRouter = new Engine::ApplicationCore::FInputRouter();
    InputRouter->AddContext(&ViewportInputContext);
}

void FEditorViewportClient::Release()
{
    // 필요한 리소스 해제
    delete InputRouter;
    InputRouter = nullptr;
}

void FEditorViewportClient::Tick(float DeltaTime, const Engine::ApplicationCore::FInputState & State)
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
