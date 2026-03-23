#pragma once

#include "Navigation/ViewportNavigationController.h"
#include "Selection/ViewportSelectionController.h"
#include "Gizmo/ViewportGizmoController.h"
#include "Interaction/ViewportInteractionState.h"
#include "Engine/ViewPort/ViewportClient.h"
#include "Input/NavigationInputContext.h"
#include "Input/SelectionInputContext.h"
#include "Renderer/EditorRenderData.h"

struct FEditorContext;

class FEditorViewportClient : public Engine::Viewport::IViewportClient
{
  public:
    FEditorViewportClient() = default;
    ~FEditorViewportClient() override = default;

    void Create() override;
    void Release() override;

    void Initialize(FScene* Scene, uint32 ViewportWidth, uint32 ViewportHeight) override;

    void Tick(float DeltaTime, const Engine::ApplicationCore::FInputState& State) override;
    void Draw() override;

    void HandleInputEvent(const Engine::ApplicationCore::FInputEvent& Event,
                          const Engine::ApplicationCore::FInputState& State) override;

    void BuildRenderData(FEditorRenderData& OutRenderData) const;

    void OnResize(uint32 InWidth, uint32 InHeight);
    void SetEditorContext(FEditorContext* InContext);
    void SetScene(FScene* InScene);
    void SyncSelectionFromContext();

    FViewportNavigationController& GetNavigationController() { return NavigationController; }
    FViewportSelectionController& GetSelectionController() { return SelectionController; }
    FViewportGizmoController& GetGizmoController() { return GizmoController; }
    FViewportInteractionState& GetInteractionState() { return InteractionState; }

    FViewportCamera& GetCamera() { return ViewportCamera; }

private:
    void DrawOutline();

  private:
    FScene* CurScene = nullptr;

    FViewportCamera ViewportCamera;

    FViewportNavigationController NavigationController;
    FViewportSelectionController SelectionController;
    FViewportGizmoController GizmoController;
    FViewportInteractionState InteractionState;

    FNavigationInputContext ViewportInputContext{&NavigationController};
    FSelectionInputContext SelectionInputContext{&SelectionController};
};
