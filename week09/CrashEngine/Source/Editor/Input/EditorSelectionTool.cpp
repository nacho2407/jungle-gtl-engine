#include "EditorSelectionTool.h"

#include "Component/CameraComponent.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

bool FEditorSelectionTool::HandleInput(float DeltaTime)
{
    (void)DeltaTime;

    if (!Owner || !Owner->GetCamera() || !Owner->GetWorld() || !Owner->GetSelectionManager())
    {
        return false;
    }

    const FEditorViewportFrameInput& Input = Owner->GetCurrentInput();
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

    World->RaycastEditorPicking(Ray, HitResult, BestActor);

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
