#pragma once
#include "Core/CoreMinimal.h"
#include "Engine/ViewPort/ViewportController.h"
#include "Engine/Component/SceneComponent.h"
#include "Gizmo/EditorGizmoTypes.h"

/*
        Gizmo에 대한 Input Context와 Viewport에서의 Gizmo의 상태를 관리하는 Controller
        축 Hover, drag begin, update, end
        translate, rotate, scale 적용
        snapping 적용
        -> GizmoInputContext에서 입력을 받아서 처리
*/

class FViewportCamera;
struct FSceneRenderData;

class FViewportGizmoController : public Engine::Viewport::IViewportController
{
    void Tick(float DeltaTime);

    // GizmoInputContext에서 호출할 훅
    void OnMouseButtonDown(const FVector2& MousePos);
    void OnMouseButtonUp();
    void OnMouseMove(const FVector2& MousePos);

    void SetViewportCamera(FViewportCamera* InCamera) { ViewportCamera = InCamera; }
    void SetSelectedObject(Engine::Component::USceneComponent* InObject)
    {
        SelectedObject = InObject;
    }
    // 기즈모 설정 변경
    void SetGizmoMode(EGizmoMode InMode) { CurrentMode = InMode; }
    void SetSnapping(bool bInEnable, float InValue)
    {
        bEnableSnapping = bInEnable;
        SnapValue = InValue;
    }

  private:
    EGizmoAxis HitTestGizmo(const FVector2& MousePos);
    void UpdateDrag(const FVector2& MousePos);

    float CalculateProjectionOffset(const Geometry::FRay& Ray, const FVector& AxisOrigin,
                                    const FVector& AxisDir);

  private:
    EGizmoMode CurrentMode = EGizmoMode::Translate;
    EGizmoAxis ActiveAxisIndex = {EGizmoAxis::None};
    FVector    CurrentDragAxis;

    bool       bIsWorldMode = false;
    bool       bIsDragging = false;
    FVector2   StartMousePos;
    FTransform StartTransform;
    float      InitialDragOffset = 0.0f;

    bool  bEnableSnapping = false;
    float SnapValue = 10.f;

    FViewportCamera*  ViewportCamera{nullptr};
    FSceneRenderData* SceneRenderData;

    Engine::Component::USceneComponent* SelectedObject{nullptr};
};
