#pragma once

#include "Camera/ViewportCamera.h"
#include "Core/CoreMinimal.h"
#include "Engine/ViewPort/ViewportController.h"
#include "Input/ContextModeTypes.h"

/*
	오브젝트 선택 처리 계층입니다.
	클릭 선택, Ctrl 다중 선택, 빈 공간 클릭 시 선택 해제, ray-cast, pick 결과 등을 처리합니다.
    -> 사실 viewportInput과 GizmoInput 사이에서 많이 겹칩니다.
*/

class FScene;
class AActor;

class FViewportSelectionController : public Engine::Viewport::IViewportController
{
public:
    FViewportSelectionController() = default;
    ~FViewportSelectionController() override = default;

    void ClickSelect(int32 MouseX, int32 MouseY, ESelectionMode Mode);
    void BeginSelection(int32 MouseX, int32 MouseY, ESelectionMode Mode);
    void UpdateSelection(int32 MouseX, int32 MouseY);
    void EndSelection(int32 MouseX, int32 MouseY);

    void ClearSelection();

    bool                   IsSelected(AActor* Actor) const;
    const TArray<AActor*>& GetSelectedActors() const { return SelectedActors; }

    //  초기화 시점 호출
    void SetCamera(FViewportCamera* Camera) { ViewportCamera = Camera; }
    void SetActors(TArray<AActor*>* InActors) { Actors = InActors; }
    
    //  Viewport Size 바뀔 때 호출
    void SetViewportSize(uint32 Width, uint32 Height)
    {
        ViewportWidth = Width;
        ViewportHeight = Height;
    }

private:
    // Geometry::FRay BuildPickRay(int32 MouseX, int32 MouseY) const;
    AActor* PickActor(int32 MouseX, int32 MouseY) const;

    void SelectSingle(AActor* Actor);
    void AddSelection(AActor* Actor);
    void RemoveSelection(AActor* Actor);
    void ToggleSelection(AActor* Actor);

private:
    TArray<AActor*>* Actors = nullptr;
    FViewportCamera * ViewportCamera = nullptr;
    
    uint32 ViewportWidth = 0;
    uint32 ViewportHeight = 0;

    int32          SelectionStartX = 0;
    int32          SelectionStartY = 0;
    ESelectionMode CurSelectionMode = ESelectionMode::Replace;

    bool bIsDraggingSelection = false;

    TArray<AActor*> SelectedActors;
};