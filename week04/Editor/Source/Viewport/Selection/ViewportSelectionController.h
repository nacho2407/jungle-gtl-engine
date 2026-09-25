#pragma once

#include "Camera/ViewportCamera.h"
#include "Core/CoreMinimal.h"
#include "Engine/ViewPort/ViewportController.h"
#include "Input/ContextModeTypes.h"

class FScene;
class AActor;
struct FEditorContext;

class FViewportSelectionController : public Engine::Viewport::IViewportController
{
  public:
    FViewportSelectionController() = default;
    ~FViewportSelectionController() override = default;

    void ClickSelect(int32 MouseX, int32 MouseY, ESelectionMode Mode);
    void BeginSelection(int32 MouseX, int32 MouseY, ESelectionMode Mode);
    void UpdateSelection(int32 MouseX, int32 MouseY);
    void EndSelection(int32 MouseX, int32 MouseY);

    void SelectActor(AActor* Actor, ESelectionMode Mode);
    void ClearSelection();
    void SyncSelectionFromContext();

    bool                   IsSelected(AActor* Actor) const;
    const TArray<AActor*>& GetSelectedActors() const { return SelectedActors; }
    TArray<AActor*>& GetSelectedActors()  { return SelectedActors; }

    void SetCamera(FViewportCamera* Camera) { ViewportCamera = Camera; }
    void SetActors(TArray<AActor*>* InActors) { Actors = InActors; }
    void SetEditorContext(FEditorContext* InContext) { Context = InContext; }

    void SetViewportSize(uint32 Width, uint32 Height)
    {
        ViewportWidth = Width;
        ViewportHeight = Height;
    }

    bool IsDraggingSelection() const { return bIsDraggingSelection; }
    void GetSelectionRect(int32 & OutStartX, int32& OutStartY, int32& OutEndX, int32& OutEndY) const
    {
        OutStartX = SelectionStartX + static_cast<int32>(ViewportCamera->GetOriginX());
        OutStartY = SelectionStartY + static_cast<int32>(ViewportCamera->GetOriginY());
        OutEndX = SelectionCurrentX + static_cast<int32>(ViewportCamera->GetOriginX());
        OutEndY = SelectionCurrentY + static_cast<int32>(ViewportCamera->GetOriginY());
    }

private:
    // Geometry::FRay BuildPickRay(int32 MouseX, int32 MouseY) const;
    AActor* PickActor(int32 MouseX, int32 MouseY) const;

    void SelectSingle(AActor* Actor);
    void AddSelection(AActor* Actor);
    void RemoveSelection(AActor* Actor);
    void ToggleSelection(AActor* Actor);

    void    ResetSelection();
    void    ApplySelectionState(AActor* Actor, bool bSelected) const;
    AActor* ResolveActorFromContextSelection() const;
    void    UpdatePrimarySelection() const;

    bool ProjectWorldToScreen(const FVector& WorldPos, FVector2& OutScreenPos) const;

  private:
    FEditorContext*  Context = nullptr;
    TArray<AActor*>* Actors = nullptr;
    FViewportCamera* ViewportCamera = nullptr;

    uint32 ViewportWidth = 0;
    uint32 ViewportHeight = 0;

    int32          SelectionStartX = 0;
    int32          SelectionStartY = 0;
    int32          SelectionCurrentX = 0;
    int32          SelectionCurrentY = 0;
    ESelectionMode CurSelectionMode = ESelectionMode::Replace;

    bool bIsDraggingSelection = false;

    TArray<AActor*> SelectedActors;
};
