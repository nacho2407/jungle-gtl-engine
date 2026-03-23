#include "Viewport/EditorViewportClient.h"

#include "ApplicationCore/Input/InputRouter.h"
#include "Editor/EditorContext.h"
#include "Engine/Scene.h"
#include "Renderer/Types/EditorShowFlags.h"

void FEditorViewportClient::Create()
{
    InputRouter = new Engine::ApplicationCore::FInputRouter();
    InputRouter->AddContext(&ViewportInputContext);
    InputRouter->AddContext(&SelectionInputContext);

    NavigationController.SetCamera(&ViewportCamera);
    SelectionController.SetCamera(&ViewportCamera);

    ViewportCamera.SetProjectionType(EViewportProjectionType::Perspective);
    ViewportCamera.SetFOV(3.141592f * 0.5f);
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
    SetScene(Scene);

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

    SelectionController.SyncSelectionFromContext();
    NavigationController.Tick(DeltaTime);
}

void FEditorViewportClient::Draw() {}

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
    // TODO
    OutRenderData.Gizmo.GizmoType = EGizmoType::Translation;
    OutRenderData.Gizmo.Highlight = EGizmoHighlight::None;
    OutRenderData.Gizmo.Frame = FMatrix::Identity;
    OutRenderData.ShowFlags = EEditorShowFlags::SF_Grid | EEditorShowFlags::SF_WorldAxes |
                              EEditorShowFlags::SF_SelectionOutline |
                              EEditorShowFlags::SF_ObjectLabels;
}

void FEditorViewportClient::OnResize(uint32 Width, uint32 Height)
{
    ViewportCamera.OnResize(Width, Height);
    SelectionController.SetViewportSize(Width, Height);
}

void FEditorViewportClient::SetEditorContext(FEditorContext* InContext)
{
    SelectionController.SetEditorContext(InContext);
}

void FEditorViewportClient::SetScene(FScene* InScene)
{
    CurScene = InScene;
    SelectionController.SetActors(InScene != nullptr ? InScene->GetActors() : nullptr);
}

void FEditorViewportClient::SyncSelectionFromContext()
{
    SelectionController.SyncSelectionFromContext();
}
