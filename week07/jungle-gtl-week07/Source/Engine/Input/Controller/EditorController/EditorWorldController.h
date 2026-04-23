#pragma once
#include "BaseEditorController.h"
#include "Editor/Viewport/ViewportCamera.h"

class FSelectionManager;
class UGizmoComponent;
class UWorld;

// Note: Editor Viewport uses FViewportCamera
class FEditorWorldController : public IBaseEditorController
{
  public:
    void Tick(float InDeltaTime) override;
    void OnMouseMove(float DeltaX, float DeltaY) override {}
    void OnMouseMoveAbsolute(float X, float Y) override;
    void OnLeftMouseClick(float X, float Y) override;    // LMB down
    void OnLeftMouseDragEnd(float X, float Y) override;  // LMB drag released
    void OnLeftMouseButtonUp(float X, float Y) override; // LMB up (no drag)
    void OnRightMouseClick(float DeltaX, float DeltaY) override;
    void OnLeftMouseDrag(float X, float Y) override; // X/Y = viewport-local pos
    void OnRightMouseDrag(float DeltaX, float DeltaY) override;
    void OnMiddleMouseDrag(float DeltaX, float DeltaY) override;
    void OnKeyPressed(int VK) override;
    void OnKeyDown(int VK) override;
    void OnKeyReleased(int VK) override;
    void OnWheelScrolled(float Notch) override;

    void SetSelectionManager(FSelectionManager* InSM);
    void SetSelectionManager(FSelectionManager& InSM);
    void NullifySelectionManager()
    {
        SelectionManager = nullptr;
        Gizmo = nullptr;
    }
    void SetGizmo(UGizmoComponent* InGizmo);
    void SetGizmo(UGizmoComponent& InGizmo);
    void NullifyGizmo() { Gizmo = nullptr; }
    void SetCamera(FViewportCamera* InCamera);
    void SetCamera(FViewportCamera& InCamera);
    void NullifyCamera() { Camera = nullptr; }

    float GetMoveSpeed() const { return MoveSpeed; }
    void  SetMoveSpeed(float InSpeed) { MoveSpeed = InSpeed; }
    FVector GetTargetLocation() const { return TargetLocation; }
    void  SetTargetLocation(FVector InTargetLoc) { TargetLocation = InTargetLoc; }
    void  ResetTargetLocation()
    {
        if (Camera)
            TargetLocation = Camera->GetLocation();
    }
    void SetWorld(UWorld* InWorld)
    {
        if (InWorld)
            World = InWorld;
    }
    void NullifyWorld() { World = nullptr; }

  private:
    void UpdateCameraRotation();

  private:
    FSelectionManager* SelectionManager = nullptr;
    FViewportCamera*   Camera = nullptr;
    UGizmoComponent*   Gizmo = nullptr;
    UWorld*            World = nullptr;

    float   Yaw   = 0.f;
    float   Pitch = 0.f;
    float   MoveSpeed = 15.f;
    FVector TargetLocation;
    bool    bTargetLocationInitialized = false;
};
