#pragma once

#include "Navigation/ViewportNavigationController.h"
#include "Selection/ViewportSelectionController.h"
#include "Gizmo/ViewportGizmoController.h"
#include "Interaction/ViewportInteractionState.h"
#include "Engine/ViewPort/ViewportClient.h"
#include "Input/NavigationInputContext.h"
#include "Renderer/EditorRenderData.h"


/*
        하위에 Controller 계층과 Render Setting 계층을 포함하는 Viewport의 최상위 관리 계층입니다.
        InputRouter를 소유하고 있습니다.
*/

class FEditorViewportClient : public Engine::Viewport::IViewportClient
{
  public:
    FEditorViewportClient() = default;
    ~FEditorViewportClient() override = default;

    void Create() override;
    void Release() override;
    
    void Initialize(FScene * Scene, uint32 ViewportWidth, uint32 ViewportHeight) override;

    void Tick(float DeltaTime, const Engine::ApplicationCore::FInputState & State) override;
    void Draw() override;

    void HandleInputEvent(const Engine::ApplicationCore::FInputEvent& Event,
                          const Engine::ApplicationCore::FInputState& State) override;

    void BuildRenderData(FEditorRenderData& OutRenderData) const;
    
    void OnResize(uint32 InWidth, uint32 InHeight);

    FViewportNavigationController& GetNavigationController() { return NavigationController; }
    FViewportSelectionController&  GetSelectionController() { return SelectionController; }
    FViewportGizmoController&      GetGizmoController() { return GizmoController; }
    FViewportInteractionState&     GetInteractionState() { return InteractionState; }

    FViewportCamera& GetCamera() { return ViewportCamera; }

  private:
    FScene * CurScene = nullptr;
    
    FViewportCamera ViewportCamera;

    FViewportNavigationController NavigationController; 
    FViewportSelectionController  SelectionController;
    FViewportGizmoController      GizmoController;
    FViewportInteractionState     InteractionState;

    FNavigationInputContext ViewportInputContext{&NavigationController};
};