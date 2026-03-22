#pragma once

#include "Navigation/ViewportNavigationController.h"
#include "Selection/ViewportSelectionController.h"
#include "Gizmo/ViewportGizmoController.h"
#include "Interaction/ViewportInteractionState.h"

/*
        하위에 Controller 계층과 Render Setting 계층을 포함하는 Viewport의 최상위 관리 계층입니다.
*/

class FEditorViewportClient
{
  public:
    FEditorViewportClient() = default;
    ~FEditorViewportClient() = default;

    void Create();
    void Release();

    void Tick(float DeltaTime);

    FViewportNavigationController& GetNavigationController() { return NavigationController; }
    FViewportSelectionController&  GetSelectionController() { return SelectionController; }
    FViewportGizmoController& GetGizmoController() { return GizmoController; }
    FViewportInteractionState&     GetInteractionState() { return InteractionState; }

  private:
    FViewportNavigationController NavigationController;
    FViewportSelectionController  SelectionController;
    FViewportGizmoController      GizmoController;
    FViewportInteractionState     InteractionState;
};