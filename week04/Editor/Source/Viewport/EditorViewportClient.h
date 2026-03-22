#pragma once

#include "Navigation/ViewportNavigationController.h"
#include "Selection/ViewportSelectionController.h"
#include "Gizmo/ViewportGizmoController.h"
#include "Interaction/ViewportInteractionState.h"
#include "Engine/ViewPort/ViewportClient.h"
#include "Input/ViewportInputContext.h"

/*
        하위에 Controller 계층과 Render Setting 계층을 포함하는 Viewport의 최상위 관리 계층입니다.
        InputRouter를 소유하고 있습니다.
*/

class FEditorViewportClient : public Engine::Viewport::IViewportClient
{
  public:
    FEditorViewportClient() = default;
    ~FEditorViewportClient() override = default;

    void Create();
    void Release();

    void Tick(float DeltaTime, const Engine::ApplicationCore::FInputState & State) override;
    void Draw() override;

    void HandleInputEvent(const Engine::ApplicationCore::FInputEvent& Event,
                          const Engine::ApplicationCore::FInputState& State) override;


    FViewportNavigationController& GetNavigationController() { return NavigationController; }
    FViewportSelectionController&  GetSelectionController() { return SelectionController; }
    FViewportGizmoController&      GetGizmoController() { return GizmoController; }
    FViewportInteractionState&     GetInteractionState() { return InteractionState; }

  private:
    FViewportNavigationController NavigationController; 
    FViewportSelectionController  SelectionController;
    FViewportGizmoController      GizmoController;
    FViewportInteractionState     InteractionState;

    FViewportInputContext ViewportInputContext{&NavigationController};
};