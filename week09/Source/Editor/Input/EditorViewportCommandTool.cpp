#include "EditorViewportCommandTool.h"

#include "Engine/Component/GizmoComponent.h"
#include "Engine/GameFramework/AActor.h"
#include "Engine/GameFramework/World.h"
#include "Engine/Object/Object.h"

#include "Editor/EditorEngine.h"
#include "Editor/Input/EditorViewportInputController.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/Viewport/EditorViewportClient.h"

bool FEditorViewportCommandTool::HandleInput(float DeltaTime)
{
    (void) DeltaTime;

    if (!Owner || !Controller)
    {
        return false;
    }

    const FEditorViewportFrameInput& Input = Controller->GetCurrentInput();

	UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);
    if (!EditorEngine)
    {
        return false;
    }

    if (EditorEngine->IsPlayingInEditor() && Input.KeyPressed[VK_ESCAPE])
    {
        EditorEngine->RequestEndPlayMap();
		return true;
    }

	FSelectionManager* SelectionManager = Owner->GetSelectionManager();

    if (SelectionManager && Input.KeyPressed[VK_DELETE])
    {
        UWorld* World = Owner->GetWorld();
        if (!World)
        {
            return false;
        }

        const TArray<AActor*> ActorsToDelete = SelectionManager->GetSelectedActors();
        if (ActorsToDelete.empty())
        {
            return false;
        }

        SelectionManager->ClearSelection();

        World->BeginDeferredPickingBVHUpdate();
        for (AActor* Actor : ActorsToDelete)
        {
            if (Actor && Actor->GetWorld() == World)
            {
                World->DestroyActor(Actor);
            }
        }
        World->EndDeferredPickingBVHUpdate();

        return true;
    }

    if (SelectionManager && Input.Modifiers.bCtrl && Input.KeyDown['A'])
    {
        if (Input.KeyPressed['A'])
        {
            UWorld* World = Owner->GetWorld();
            if (!World)
            {
                return false;
            }

            SelectionManager->ClearSelection();
            for (AActor* Actor : World->GetActors())
            {
                if (Actor)
                {
                    SelectionManager->ToggleSelect(Actor);
                }
            }
        }

        return true;
    }

    if (SelectionManager && Input.Modifiers.bCtrl && Input.KeyDown['D'])
    {
        if (Input.KeyPressed['D'])
        {
            const TArray<AActor*> ToDuplicate = SelectionManager->GetSelectedActors();
            if (!ToDuplicate.empty())
            {
                TArray<AActor*> NewSelection;
                for (AActor* Src : ToDuplicate)
                {
                    if (!Src)
                        continue;
                    AActor* Dup = Cast<AActor>(Src->Duplicate(nullptr));
                    if (Dup)
                    {
                        NewSelection.push_back(Dup);
                    }
                }
                SelectionManager->ClearSelection();
                for (AActor* Actor : NewSelection)
                {
                    SelectionManager->ToggleSelect(Actor);
                }
            }
        }
		
		return true;
    }

    if (Input.KeyReleased[VK_SPACE])
    {
        if (UGizmoComponent* Gizmo = Owner->GetGizmo())
        {
            Gizmo->SetNextMode();
            return true;
        }
    }

    return false;
}
