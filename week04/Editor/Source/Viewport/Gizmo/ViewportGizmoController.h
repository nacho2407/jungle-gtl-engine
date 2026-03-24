#pragma once
#include "Core/CoreMinimal.h"
#include "Engine/ViewPort/ViewportController.h"
#include "Engine/Component/Core/SceneComponent.h"
#include "Gizmo/EditorGizmoTypes.h"
#include "Renderer/Types/PickResult.h"

/*
        Gizmo에 대한 Input Context와 Viewport에서의 Gizmo의 상태를 관리하는 Controller
        축 Hover, drag begin, update, end
        translate, rotate, scale 적용
        snapping 적용
        -> GizmoInputContext에서 입력을 받아서 처리
*/

class FViewportCamera;
class FEditorViewportClient;
class FViewportSelectionController;
class AActor;

class FViewportGizmoController : public Engine::Viewport::IViewportController
{
  public:
    void Tick(float DeltaTime);

    // GizmoInputContext에서 호출할 훅
    bool OnMouseButtonDown(int32 MouseX, int32 MouseY);
    void OnMouseButtonUp();
    void OnMouseMove(int32 MouseX, int32 MouseY);
    bool IsDragging() { return bIsDragging; }

    EGizmoType GetGizmoType() const { return GizmoType; }
    void       ChangeGizmoType()
    {
        if (GizmoType == EGizmoType::Translation)
            GizmoType = EGizmoType::Rotation;
        else if (GizmoType == EGizmoType::Rotation)
            GizmoType = EGizmoType::Scaling;
        else if (GizmoType == EGizmoType::Scaling)
            GizmoType = EGizmoType::Translation;
    }
    void    ChangeWorldMode();
    FMatrix    GetMatrix() const;

    void SetCamera(FViewportCamera* InCamera) { ViewportCamera = InCamera; }
    void SetViewportClient(FEditorViewportClient* InClient) { ViewportClient = InClient; }
    void SetViewportSelectionController(FViewportSelectionController* InControllelr)
    {
        ViewportSelectionController = InControllelr;
    }

    void SetSelectedActor(AActor* InActor) { SelectedActor = InActor; }
    AActor* GetSelectedActor() const  { return SelectedActor; }

    // 기즈모 설정 변경
    // void SetGizmoMode(EGizmoMode InMode) { CurrentMode = InMode; }
    void SetSnapping(bool bInEnable, float InValue)
    {
        bEnableSnapping = bInEnable;
        SnapValue = InValue;
    }

    bool bIsDrawed{false};

    public:
    bool bIsWorldMode = false;

  private:
    bool HitTestGizmo(int32 MouseX, int32 MouseY);
    void UpdateDrag(int32 MouseX, int32 MouseY);

    float CalculateProjectionOffset(const Geometry::FRay& Ray, const FVector& AxisOrigin,
                                    const FVector& AxisDir);

  private:
    EGizmoType GizmoType = EGizmoType::Translation;
    EAxis      Axis = EAxis::X;
    FVector    CurrentDragAxis;

    FPickResult PickData;

    
    
    bool       bIsDragging = false;
    int32      StartMousePosX;
    int32      StartMousePosY;
    FTransform StartTransform;
    float      InitialDragOffset = 0.0f;

    bool  bEnableSnapping = false;
    float SnapValue = 10.f;

    FEditorViewportClient* ViewportClient{nullptr};
    FViewportCamera*       ViewportCamera{nullptr};
    FViewportSelectionController* ViewportSelectionController{nullptr};

    AActor* SelectedActor{nullptr};
};
