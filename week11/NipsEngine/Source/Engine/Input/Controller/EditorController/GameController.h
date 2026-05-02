#pragma once
#include "BaseEditorController.h"
#include <functional>

class FViewportCamera;
class APlayerController;

//  기존에 PIE Controller였음 이름이
class FGameController : public IBaseEditorController
{
  public:
    void Tick(float InDeltaTime) override;
    void OnMouseMove(float DeltaX, float DeltaY) override;
    void OnMouseMoveAbsolute(float X, float Y) override;
    void OnLeftMouseClick(float X, float Y) override;
    void OnLeftMouseDragEnd(float X, float Y) override;
    void OnLeftMouseButtonUp(float X, float Y) override;
    void OnRightMouseClick(float DeltaX, float DeltaY) override;
    void OnLeftMouseDrag(float X, float Y) override;
    void OnRightMouseDrag(float DeltaX, float DeltaY) override;
    void OnMiddleMouseDrag(float DeltaX, float DeltaY) override;
    void OnKeyPressed(int VK) override;
    void OnKeyDown(int VK) override;
    void OnKeyReleased(int VK) override;
    void OnWheelScrolled(float Notch) override;

    void SetCamera(FViewportCamera* InCamera);
    void SetCamera(FViewportCamera& InCamera);
    void NullifyCamera() { Camera = nullptr; }
    void ResetTargetLocation();
    void SetPlayerController(APlayerController* InController) { PlayerController = InController; }
    void ClearPlayerController() { PlayerController = nullptr; }
    APlayerController* GetPlayerController() const { return PlayerController; }

    void SetEndPIECallback(std::function<void()> Callback) { OnRequestEndPIE = std::move(Callback); }
    void ClearEndPIECallback() { OnRequestEndPIE = nullptr; }

	FVector GetTargetLocation() const { return TargetLocation; }
	void SetTargetLocation(FVector InTargetLoc) { TargetLocation = InTargetLoc; }

  private:
	void UpdateCameraRotation();
  private:
    FViewportCamera*      Camera = nullptr;
    APlayerController*    PlayerController = nullptr;
    std::function<void()> OnRequestEndPIE;

    float                 Yaw = 0.f;
    float                 Pitch = 0.f;
    float                 MoveSpeed = 15.f;
    FVector               TargetLocation;
    bool                  bTargetLocationInitialized = false;
};
