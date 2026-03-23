#include "Viewport/EditorViewportClient.h"
#include "ApplicationCore/Input/InputRouter.h"
#include "Engine/Scene.h"
#include "Renderer/Types/EditorShowFlags.h"

void FEditorViewportClient::Create()
{
    // 뷰포트 입력은 별도 라우터에 모아서 카메라/기즈모 컨텍스트로 분배합니다.
    InputRouter = new Engine::ApplicationCore::FInputRouter();
    InputRouter->AddContext(&ViewportInputContext);

    NavigationController.SetCamera(&ViewportCamera);

    ViewportCamera.SetProjectionType(EViewportProjectionType::Perspective);
    ViewportCamera.SetFOV(3.141592f * 0.5f); // TODO: 라디안 상수 정리
    ViewportCamera.SetNearPlane(0.1f);
    ViewportCamera.SetFarPlane(2000.0f);
    ViewportCamera.SetLocation(FVector(-10.0f, 5.0f, 50.0f));
    ViewportCamera.SetRotation(FRotator(0.0f, 0.0f, 0.0f));
}

void FEditorViewportClient::Release()
{
    if (InputRouter)
    {
        delete InputRouter;
        InputRouter = nullptr;
    }
}

void FEditorViewportClient::Initialize(FScene* Scene, uint32 ViewportWidth, uint32 ViewportHeight)
{
    CurScene = Scene;

    ViewportCamera.OnResize(ViewportWidth, ViewportHeight);

    SelectionController.SetActors(Scene->GetActors());
    SelectionController.SetCamera(&ViewportCamera);
    SelectionController.SetViewportSize(ViewportWidth, ViewportHeight);
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
    // 매 프레임 필요한 뷰포트 보조 드로우 호출 지점입니다.
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
    // OutRenderData.Gizmo.GizmoType = EGizmoType::Translation;
     OutRenderData.Gizmo.GizmoType = EGizmoType::Rotation;
    //OutRenderData.Gizmo.GizmoType = EGizmoType::Scaling;
    OutRenderData.Gizmo.Highlight = EGizmoHighlight::None;
    OutRenderData.Gizmo.Frame = FMatrix::Identity;
    OutRenderData.ShowFlags = EEditorShowFlags::SF_Grid | EEditorShowFlags::SF_WorldAxes |
                              EEditorShowFlags::SF_Gizmo | EEditorShowFlags::SF_SelectionOutline |
                              EEditorShowFlags::SF_ObjectLabels;
}

void FEditorViewportClient::OnResize(uint32 Width, uint32 Height)
{
    // 창 리사이즈를 카메라에 전달해 aspect ratio와 projection matrix를 갱신합니다.
    ViewportCamera.OnResize(Width, Height);

    SelectionController.SetViewportSize(Width, Height);
}
