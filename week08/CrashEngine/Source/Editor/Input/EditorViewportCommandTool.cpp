#include "EditorViewportCommandTool.h"

#include "Engine/Component/GizmoComponent.h"
#include "Engine/GameFramework/AActor.h"
#include "Engine/Object/Object.h"

#include "Editor/EditorEngine.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/Viewport/EditorViewportClient.h"

bool FEditorViewportCommandTool::HandleInput(float DeltaTime)
{
    (void) DeltaTime;

    if (!Owner)
    {
        return false;
    }

    const FEditorViewportFrameInput& Input = Owner->GetCurrentInput();

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

    if (SelectionManager && Input.Modifiers.bCtrl && Input.KeyPressed['D'])
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
		
		return true;
    }

	UGizmoComponent* Gizmo = Owner->GetGizmo();

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
