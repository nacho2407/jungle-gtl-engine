#pragma once

#include "Core/CoreMinimal.h"
#include "ApplicationCore/Input/InputContext.h"
#include "Viewport/Gizmo/ViewportGizmoController.h"

class FViewportNavigationController;

class FGizmoInputContext : public Engine::ApplicationCore::IInputContext
{
  public:
    FGizmoInputContext(FViewportGizmoController* InGizmoController);
    ~FGizmoInputContext() override = default;

    // 현재는 Literal 저장
    int32 GetPriority() const override { return 50; }
    bool  HandleEvent(const Engine::ApplicationCore::FInputEvent& Event,
                      const Engine::ApplicationCore::FInputState& State) override;
    void  Tick(const Engine::ApplicationCore::FInputState& State) override;

    void SetDeltaTime(float InDeltaTime) { DeltaTime = InDeltaTime; }
    void SetGizmoController(FViewportGizmoController* InController)
    {
        GizmoController = InController;
    }
    void SetNavigationController(FViewportNavigationController* InNavigationController)
    {
        NavigationController = InNavigationController;
    }

  private:
    FViewportGizmoController* GizmoController = nullptr;
    FViewportNavigationController * NavigationController = nullptr;
    
    bool bFollowViewportCamera = false;
    float ShiftDragTranslationScale = 0.35f;    //  Shift + 이동 시 속도 조절
    
    float DeltaTime = 0.016f; 
};
