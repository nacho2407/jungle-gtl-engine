#include "EditorGlobalController.h"

#include "Editor/Editor.h"
#include "Editor/EditorContext.h"
#include "Engine/Component/Core/SceneComponent.h"
#include "Engine/Game/Actor.h"
#include "Engine/Scene.h"
#include "Viewport/Selection/ViewportSelectionController.h"

bool FEditorGlobalController::CanDeleteSelectedActors() const
{
    if (Context == nullptr)
    {
        return false;
    }

    if (!Context->SelectedActors.empty())
    {
        return true;
    }

    if (Cast<AActor>(Context->SelectedObject) != nullptr)
    {
        return true;
    }

    auto* SelectedComponent = Cast<Engine::Component::USceneComponent>(Context->SelectedObject);
    if (SelectedComponent == nullptr || Scene == nullptr)
    {
        return false;
    }

    const TArray<AActor*>* SceneActors = Scene->GetActors();
    if (SceneActors == nullptr)
    {
        return false;
    }

    for (AActor* Actor : *SceneActors)
    {
        if (Actor != nullptr && Actor->GetRootComponent() == SelectedComponent)
        {
            return true;
        }
    }

    return false;
}

bool FEditorGlobalController::DeleteSelectedActors()
{
    if (Scene == nullptr || Context == nullptr)
    {
        return false;
    }

    TArray<AActor*> ActorsToDelete = Context->SelectedActors;
    if (ActorsToDelete.empty())
    {
        if (AActor* SelectedActor = Cast<AActor>(Context->SelectedObject))
        {
            ActorsToDelete.push_back(SelectedActor);
        }
        else if (Engine::Component::USceneComponent* SelectedComponent =
                     Cast<Engine::Component::USceneComponent>(Context->SelectedObject))
        {
            const TArray<AActor*>* SceneActors = Scene->GetActors();
            if (SceneActors != nullptr)
            {
                for (AActor* Actor : *SceneActors)
                {
                    if (Actor != nullptr && Actor->GetRootComponent() == SelectedComponent)
                    {
                        ActorsToDelete.push_back(Actor);
                        break;
                    }
                }
            }
        }
    }

    if (ActorsToDelete.empty())
    {
        return false;
    }

    if (SelectionController != nullptr)
    {
        SelectionController->ClearSelection();
    }

    for (AActor* Actor : ActorsToDelete)
    {
        Scene->RemoveActor(Actor);
    }

    if (Context->Editor != nullptr)
    {
        Context->Editor->MarkSceneDirty();
    }

    return true;
}

void FEditorGlobalController::NewScene()
{
    if (Context == nullptr || Context->Editor == nullptr)
    {
        return;
    }

    Context->Editor->CreateNewScene();
}

void FEditorGlobalController::OpenScene()
{
    if (Context == nullptr || Context->Editor == nullptr)
    {
        return;
    }

    Context->Editor->RequestOpenSceneDialog();
}

void FEditorGlobalController::SaveScene()
{
    if (Context == nullptr || Context->Editor == nullptr)
    {
        return;
    }

    Context->Editor->SaveCurrentSceneToDisk();
}

void FEditorGlobalController::SaveSceneAs()
{
    if (Context == nullptr || Context->Editor == nullptr)
    {
        return;
    }

    Context->Editor->RequestSaveSceneAs();
}

void FEditorGlobalController::RequestAboutPopup()
{
    if (Context == nullptr || Context->Editor == nullptr)
    {
        return;
    }

    Context->Editor->RequestAboutPopUp();
}
