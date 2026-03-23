#pragma once
#include "Core/CoreMinimal.h"
#include "Engine/ViewPort/ViewportController.h"
#include "Engine/Component/PrimitiveComponent.h"
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

    // 기즈모 설정 변경
    void SetGizmoMode(EGizmoMode InMode) { CurrentMode = InMode; }
    void SetSnapping(bool bInEnable, float InValue)
    {
        bEnableSnapping = bInEnable;
        SnapValue = InValue;
    }

  private:
    // 내부 로직: 마우스 위치로부터 어떤 축을 잡았는지 판정 (Picking)
    int32 HitTestGizmo(const FVector2& MousePos);

    // 조작량 계산 (마우스 좌표 -> 3D 이동량)
    void UpdateDrag(const FVector2& MousePos);

  private:
    EGizmoMode CurrentMode = EGizmoMode::Translate;
    int32      ActiveAxisIndex = -1; // 현재 잡고 있는 축 (0:X, 1:Y, 2:Z...)

    bool       bIsDragging = false;
    FVector2   StartMousePos;
    FTransform StartTransform; // 드래그 시작 시점의 대상 트랜스폼

    bool  bEnableSnapping = false;
    float SnapValue = 10.f;

    FViewportCamera*  ViewportCamera{nullptr};
    FSceneRenderData* SceneRenderData;

    Engine::Component::UPrimitiveComponent* SelectedObject{nullptr};
};
