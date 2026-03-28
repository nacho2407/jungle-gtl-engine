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

    // 1. 다중 선택된 액터가 있는 경우 삭제 가능
    if (!Context->SelectedActors.empty())
    {
        return true;
    }

    // 2. 선택된 객체가 액터인 경우 삭제 가능
    if (Cast<AActor>(Context->SelectedObject) != nullptr)
    {
        return true;
    }

    // 3. 선택된 객체가 컴포넌트인 경우
    if (auto* SelectedComponent = Cast<Engine::Component::USceneComponent>(Context->SelectedObject))
    {
        AActor* Owner = SelectedComponent->GetOwnerActor();
        // 루트가 아닌 컴포넌트만 삭제 가능 대상으로 판정합니다.
        if (Owner != nullptr && SelectedComponent != Owner->GetRootComponent())
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

    // 1. 컴포넌트 개별 삭제 로직 처리
    if (Engine::Component::USceneComponent* SelectedComponent =
            Cast<Engine::Component::USceneComponent>(Context->SelectedObject))
    {
        AActor* Owner = SelectedComponent->GetOwnerActor();
        
        // RootComponent가 아닌 일반 컴포넌트일 경우에만 컴포넌트 삭제를 수행합니다.
        if (Owner != nullptr && SelectedComponent != Owner->GetRootComponent())
        {
            Owner->RemoveOwnedComponent(SelectedComponent);
            
            if (SelectionController != nullptr) SelectionController->ClearSelection();
            Context->SelectedObject = Owner; // 부모 액터를 선택 상태로 변경
            if (Context->Editor != nullptr) Context->Editor->MarkSceneDirty();
            return true;
        }

        // 루트 컴포넌트인 경우, 액터 삭제로 넘어가지 않고 여기서 처리를 중단합니다.
        // (사용자가 의도치 않게 액터를 지우는 것을 방지)
        return false;
    }

    // 2. 액터 삭제 로직 (순수하게 액터가 선택되었을 때만 작동하도록 제한)
    TArray<AActor*> ActorsToDelete = Context->SelectedActors;
    if (ActorsToDelete.empty())
    {
        if (AActor* SelectedActor = Cast<AActor>(Context->SelectedObject))
        {
            ActorsToDelete.push_back(SelectedActor);
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
