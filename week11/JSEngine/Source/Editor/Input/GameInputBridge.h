#pragma once
#include "BaseEditorController.h"

class FViewportCamera;
class APlayerController;
struct FViewportInputContext;

class FGameInputBridge : public IBaseEditorController
{
  public:
    void Tick(float InDeltaTime) override;
    void OnMouseMove(float DeltaX, float DeltaY) override;
    void OnKeyDown(int VK) override;
    void ProcessInputContext(const FViewportInputContext& Context);

    void SetCamera(FViewportCamera* InCamera);
    void SetCamera(FViewportCamera& InCamera);
    void NullifyCamera() { Camera = nullptr; }
    void ResetTargetLocation();
    void SetPlayerController(APlayerController* InController) { PlayerController = InController; }
    void ClearPlayerController() { PlayerController = nullptr; }
    APlayerController* GetPlayerController() const { return PlayerController; }

	FVector GetTargetLocation() const { return TargetLocation; }
	void SetTargetLocation(FVector InTargetLoc) { TargetLocation = InTargetLoc; }
  private:
    FViewportCamera*      Camera = nullptr;
    APlayerController*    PlayerController = nullptr;

    FVector               TargetLocation;
    bool                  bTargetLocationInitialized = false;
};
