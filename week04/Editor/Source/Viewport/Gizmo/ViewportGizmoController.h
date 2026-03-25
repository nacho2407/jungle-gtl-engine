#pragma once
#include "Core/CoreMinimal.h"
#include "Engine/ViewPort/ViewportController.h"
#include "Engine/Component/Core/SceneComponent.h"
#include "Gizmo/EditorGizmoTypes.h"
#include "Renderer/Types/PickResult.h"
#include <ApplicationCore/Input/InputState.h>

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
    EGizmoHighlight GetGizmoHighlight() const { return GizmoHighlight; }
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
    FMatrix GetMatrix() const;

    void SetCamera(FViewportCamera* InCamera) { ViewportCamera = InCamera; }
    void SetViewportClient(FEditorViewportClient* InClient) { ViewportClient = InClient; }
    void SetViewportSelectionController(FViewportSelectionController* InControllelr)
    {
        ViewportSelectionController = InControllelr;
    }

    void    SetSelectedActor(AActor* InActor) { LastSelectedActor = InActor; }
    AActor* GetSelectedActor() const { return LastSelectedActor; }

    /*void    SetSelectedActors(TArray<AActor*>* InActors) { SelectedActors = InActors; }
    TArray<AActor*>* GetSelectedActors() const { return SelectedActors; }*/

    // 기즈모 설정 변경
    // void SetGizmoMode(EGizmoMode InMode) { CurrentMode = InMode; }
    void SetTranslateSnapping(bool bInEnable, float InValue)
    {
        bEnableTranslationSnap = bInEnable;
        TranslationSnapValue = InValue;
    }
    void GetTranslateSnapping(bool &bInEnable, float &InValue) const
    {
        bInEnable = bEnableTranslationSnap;
        InValue = TranslationSnapValue;
    }
    
    void SetRotateSnapping(bool bInEnable, float InValue)
    {
        bEnableRotationSnap = bInEnable;
        RotationSnapValue = InValue;
    }
    void GetRotateSnapping(bool &bInEnable, float &InValue) const
    {
        bInEnable = bEnableRotationSnap;
        InValue = RotationSnapValue;
    }
    
    void SetScaleSnapping(bool bInEnable, float InValue)
    {
        bEnableScaleSnap = bInEnable;
        ScaleSnapValue = InValue;
    }
    void GetScaleSnapping(bool &bInEnable, float &InValue) const
    {
        bInEnable = bEnableScaleSnap;
        InValue = ScaleSnapValue;
    }
    
    void SetTranslationDragScale(float InScale)
    {
        TranslationDragScale = FMath::Clamp(InScale, 0.01f, 1.0f);
    }

    bool bIsDrawed{false};
    float GizmoScale{1.0f};
    
    /* For Navigation */
    FVector ConsumeDelta()
    {
        FVector Temp = LastFrameDelta;
        LastFrameDelta = FVector::ZeroVector;
        return Temp;
    }

  public:
    bool bIsWorldMode = false;

  private:
    bool HitTestGizmo(int32 MouseX, int32 MouseY);
    void UpdateDrag(int32 MouseX, int32 MouseY);

    float   CalculateProjectionOffset(const Geometry::FRay& Ray, const FVector& AxisOrigin,
                                      const FVector& AxisDir);
    FVector RayPlaneIntersection(const Geometry::FRay& Ray, const FVector& PlaneOrigin,
                                 const FVector& PlaneNormal);
    FVector2 ProjectWorldToScreen(const FVector& WorldPos);

  private:
    EGizmoType GizmoType{EGizmoType::Translation};
    EAxis      Axis{EAxis::X};
    EGizmoHighlight GizmoHighlight{EGizmoHighlight::None};
    FVector    CurrentDragAxis;
    FVector        ReferenceAxis;
    
    FVector LastFrameDelta = FVector::ZeroVector;

    FVector2        ReferenceAxis2D;
    FVector         PivotOrigin;
    FVector2        PivotOrigin2D;
    float           ChangeSensitivity = 0.1f;

    FPickResult PickData;

    bool       bIsDragging = false;
    int32      StartMousePosX = 0;
    int32      StartMousePosY = 0;
    FTransform StartTransform;
    float      InitialDragOffset{0.0f};
    float      InitialProjectionT{0.f};

    FVector PlaneNormal;
    FVector InitialPlaneHit;
    
    FVector    RotationStartVector;

    FVector InitialHitDir;

    // Translation
    bool  bEnableTranslationSnap = true;
    float TranslationSnapValue = 1.f;

    // Rotation
    bool  bEnableRotationSnap = true;
    float RotationSnapValue = 5.f; // degree
    
    // Scaling
    bool  bEnableScaleSnap = true;
    float ScaleSnapValue = 0.25f;
    
    // 수정: 누적 snapping용 상태
    float DragStartOffset = 0.0f;
    float PrevSnappedOffset = 0.0f;
    FVector PrevSnappedCenterDelta = FVector::ZeroVector;

    // 수정: multi transform 저장
    TArray<FVector> InitialActorLocations;
    TArray<FVector> InitialActorScales;
    TArray<FVector> InitialActorOffsets; // pivot 기준
    
    float TranslationDragScale = 1.0f;

    FEditorViewportClient*        ViewportClient{nullptr};
    FViewportCamera*              ViewportCamera{nullptr};
    FViewportSelectionController* ViewportSelectionController{nullptr};

    AActor* LastSelectedActor{nullptr};
    

    static constexpr int32 DragThreshold = 5; // 픽셀 단위 드래그 시작 임계값
};
