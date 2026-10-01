#include "EditorSelectionTool.h"

#include "Component/CameraComponent.h"
#include "Editor/Input/EditorViewportInputController.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Engine/Profiling/PlatformTime.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

bool FEditorSelectionTool::HandleInput(float DeltaTime)
{
    (void)DeltaTime;

    if (!Owner || !Controller || !Owner->GetCamera() || !Owner->GetWorld() || !Owner->GetSelectionManager())
    {
        return false;
    }

    const FEditorViewportFrameInput& Input = Controller->GetCurrentInput();
    if (!Input.bLeftPressed)
    {
        return false;
    }

	if (Owner->GetWorld()->GetWorldType() == EWorldType::PIE)
    {
        return false;
    }

    FRay Ray;
    if (!BuildMouseRay(Ray))
    {
        return false;
    }

    return HandleWorldPicking(Ray);
}

bool FEditorSelectionTool::HandleWorldPicking(const FRay& Ray)
{
    UWorld* World = Owner ? Owner->GetWorld() : nullptr;
    FSelectionManager* SelectionManager = Owner ? Owner->GetSelectionManager() : nullptr;
    if (!World || !SelectionManager)
    {
        return false;
    }

    FHitResult HitResult{};
    AActor* BestActor = nullptr;

    FScopeCycleCounter PickCounter;
    World->RaycastEditorPicking(Ray, HitResult, BestActor);

    if (FOverlayStatSystem* OverlayStatSystem = Owner->GetOverlayStatSystem())
    {
        const uint64 PickCycles = PickCounter.Finish();
        const double ElapsedMs = FPlatformTime::ToMilliseconds(PickCycles);
        OverlayStatSystem->RecordPickingAttempt(ElapsedMs);
    }

    const FEditorViewportFrameInput& Input = Controller->GetCurrentInput();
    if (Input.Modifiers.bCtrl)
    {
        if (BestActor)
        {
            SelectionManager->ToggleSelect(BestActor);
        }
        return true;
    }

    if (BestActor == nullptr)
    {
        SelectionManager->ClearSelection();
    }
    else
    {
        SelectionManager->Select(BestActor);
    }

    return true;
}
