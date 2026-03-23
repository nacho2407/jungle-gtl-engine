#pragma once
#include "Core/CoreMinimal.h"
#include "Engine/ViewPort/ViewportController.h"
#include "Engine/Component/SceneComponent.h"
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
struct FEditorRenderData;
class FRendererModule;

class FViewportGizmoController : public Engine::Viewport::IViewportController
{
    void Tick(float DeltaTime);

    // GizmoInputContext에서 호출할 훅
    void OnMouseButtonDown(int32 MouseX, int32 MouseY);
    void OnMouseButtonUp();
    void OnMouseMove(int32 MouseX, int32 MouseY);

    void SetViewportCamera(FViewportCamera* InCamera) { ViewportCamera = InCamera; }
    void SetSelectedObject(Engine::Component::USceneComponent* InObject)
    {
        SelectedObject = InObject;
    }
    void SetRendererModule(FRendererModule* InRenderer) { RendererModule = InRenderer; }
    void SetEditorRenderData(FEditorRenderData* InRenderData) { EditorRenderData = InRenderData; }

    // 기즈모 설정 변경
    // void SetGizmoMode(EGizmoMode InMode) { CurrentMode = InMode; }
    void SetSnapping(bool bInEnable, float InValue)
    {
        bEnableSnapping = bInEnable;
        SnapValue = InValue;
    }

  private:
    void HitTestGizmo(int32 MouseX, int32 MouseY);
    void UpdateDrag(int32 MouseX, int32 MouseY);

    float CalculateProjectionOffset(const Geometry::FRay& Ray, const FVector& AxisOrigin,
                                    const FVector& AxisDir);

  private:
    EGizmoType GizmoType = EGizmoType::None;
    EAxis      Axis = EAxis::X;
    FVector    CurrentDragAxis;

    bool       bIsWorldMode = false;
    bool       bIsDragging = false;
    int32      StartMousePosX;
    int32      StartMousePosY;
    FTransform StartTransform;
    float      InitialDragOffset = 0.0f;

    bool  bEnableSnapping = false;
    float SnapValue = 10.f;

    FViewportCamera*   ViewportCamera{nullptr};
    FEditorRenderData* EditorRenderData;
    FRendererModule*   RendererModule;

    Engine::Component::USceneComponent* SelectedObject{nullptr};
};
