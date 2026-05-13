#include "GameInputBridge.h"
#include "Camera/ViewportCamera.h"
#include "Engine/Input/GameplayInputTypes.h"
#include "GameFramework/PlayerController.h"

void FGameInputBridge::Tick(float InDeltaTime)
{
    DeltaTime = InDeltaTime;
    if (PlayerController)
    {
        if (const FViewportCamera* RuntimeCamera = PlayerController->GetRuntimeCamera())
        {
            TargetLocation = RuntimeCamera->GetLocation();
            bTargetLocationInitialized = true;
        }
        return;
    }
}

void FGameInputBridge::ProcessInputContext(const FViewportInputContext& Context)
{
    if (!PlayerController)
    {
        return;
    }

    const FGameplayInputSnapshot Snapshot = FDefaultGameplayInputMapping::BuildSnapshot(Context);
    PlayerController->ProcessInputSnapshot(Snapshot);
}

void FGameInputBridge::OnMouseMove(float DeltaX, float DeltaY)
{
    (void)DeltaX;
    (void)DeltaY;
}

void FGameInputBridge::OnKeyDown(int VK)
{
    (void)VK;
}


void FGameInputBridge::SetCamera(FViewportCamera* InCamera)
{
    if (!InCamera)
        return;
    Camera = InCamera;

	TargetLocation = Camera->GetLocation();
    bTargetLocationInitialized = true;
}

void FGameInputBridge::SetCamera(FViewportCamera& InCamera) { SetCamera(&InCamera); }

void FGameInputBridge::ResetTargetLocation()
{
    if (Camera)
        TargetLocation = Camera->GetLocation();
}
