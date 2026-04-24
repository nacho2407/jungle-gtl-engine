// 컴포넌트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "PrimitiveComponent.h"
#include "Core/CoreTypes.h"
#include "Math/Rotator.h"
#include "Render/Execute/Context/Scene/ViewTypes.h"

class AActor;
class FPrimitiveProxy;
class FScene;

// EGizmoMode는 컴포넌트 처리에서 사용할 선택지를 정의합니다.
enum EGizmoMode
{
    Translate,
    Rotate,
    Scale,
    End
};

// UGizmoComponent 컴포넌트이다.
class UGizmoComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UGizmoComponent, UPrimitiveComponent)
    UGizmoComponent();

    bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult) override;

    FVector GetVectorForAxis(int32 Axis) const;
    void RenderGizmo() {}
    void SetTarget(AActor* NewTarget);
    void SetSelectedActors(const TArray<AActor*>* InSelectedActors) { AllSelectedActors = InSelectedActors; }
    void SetHolding(bool bHold);
    inline bool IsHolding() const { return bIsHolding; }
    inline bool IsHovered() const { return SelectedAxis != -1; }
    inline bool HasTarget() const { return TargetActor != nullptr; }
    inline AActor* GetTarget() const { return TargetActor; }
    inline int32 GetSelectedAxis() const { return SelectedAxis; }

    inline void SetPressedOnHandle(bool bPressed) { bPressedOnHandle = bPressed; }
    inline bool IsPressedOnHandle() const { return bPressedOnHandle; }

    EGizmoMode GetMode() const { return CurMode; }
    void SetAxisMask(uint32 InMask) { AxisMask = InMask; }
    uint32 GetAxisMask() const { return AxisMask; }

    // Computes the axis mask from the viewport type and gizmo mode.
    static uint32 ComputeAxisMask(ELevelViewportType ViewportType, EGizmoMode Mode);
    void UpdateHoveredAxis(int Index);
    void UpdateDrag(const FRay& Ray);
    void DragEnd();

    void SetTargetLocation(FVector NewLocation);
    void SetTargetRotation(FRotator NewRotation);
    void SetTargetScale(FVector NewScale);


    void SetNextMode();
    void UpdateGizmoMode(EGizmoMode NewMode);
    inline void SetTranslateMode() { UpdateGizmoMode(EGizmoMode::Translate); }
    inline void SetRotateMode() { UpdateGizmoMode(EGizmoMode::Rotate); }
    inline void SetScaleMode() { UpdateGizmoMode(EGizmoMode::Scale); }
    void UpdateGizmoTransform();
    float ComputeScreenSpaceScale(const FVector& CameraLocation, bool bIsOrtho = false, float OrthoWidth = 10.0f) const;
    void ApplyScreenSpaceScaling(const FVector& CameraLocation, bool bIsOrtho = false, float OrthoWidth = 10.0f);
    void SetWorldSpace(bool bWorldSpace);

    void MarkGizmoDirty(ESceneProxyDirtyFlag Flag);
    void MarkAllGizmoDirty();

    // UActorComponent Override
    void Deactivate() override;

    FMeshBuffer* GetMeshBuffer() const override;
    FMeshDataView GetMeshDataView() const override { return MeshData ? FMeshDataView::FromMeshData(*MeshData) : FMeshDataView{}; }
    FPrimitiveProxy* CreateSceneProxy() override;
    void CreateRenderState() override;
    void DestroyRenderState() override;

    // Allows the gizmo to register directly with the scene when actor ownership is not enough.
    void SetScene(FScene* InScene) { RegisteredScene = InScene; }

private:
    bool IntersectRayAxis(const FRay& Ray, FVector AxisEnd, float AxisScale, float& OutRayT);
    bool IntersectRayRotationHandle(const FRay& Ray, int32 Axis, float& OutRayT) const;

    // Control Target Method
    void HandleDrag(float DragAmount);
    void TranslateTarget(float DragAmount);
    void RotateTarget(float DragAmount);
    void ScaleTarget(float DragAmount);

    void UpdateLinearDrag(const FRay& Ray);
    void UpdateAngularDrag(const FRay& Ray);

private:
    AActor* TargetActor = nullptr;
    const TArray<AActor*>* AllSelectedActors = nullptr;
    EGizmoMode CurMode = EGizmoMode::Translate;
    FVector LastIntersectionLocation;
    const float AxisLength = 1.0f;
    float Radius = 0.1f;
    const float ScaleSensitivity = 1.0f;
    static constexpr float GizmoScreenScale = 0.15f; // Screen-space gizmo scale ratio.
    int32 SelectedAxis = -1;
    bool bIsFirstFrameOfDrag = true;
    bool bIsHolding = false;
    bool bIsWorldSpace = true;
    bool bPressedOnHandle = false;
    const FMeshData* MeshData = nullptr;
    uint32 AxisMask = 0x7;                      // Bit 0=X, 1=Y, 2=Z. Rendering proxies compute the final mask.
    FPrimitiveProxy* InnerProxy = nullptr; // Proxy used by the inner gizmo pass.
    FScene* RegisteredScene = nullptr;          // Scene used for direct gizmo registration.
};

