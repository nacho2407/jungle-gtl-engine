#pragma once

#include "Actor/AActor.h"
#include "Component/EGizmoAxis.h"
#include "Math/FVector4.h"
#include "Resource/BuiltInMeshFactory.h"
#include "Component/UCameraComponent.h"
#include "Component/UStaticMeshComponent.h"
#include "Component/EGizmoMode.h"
#include "FRay.h"

//class UStaticMesh;
//class UStaticMeshComponent;
//class UCameraComponent;
class USceneComponent;

class AGizmoActor : public AActor
{
    DECLARE_UClass(AGizmoActor, AActor)
public:
    AGizmoActor();
    virtual ~AGizmoActor() override; // 왜 가상함수로 했을까

public:
    void Initialize(UStaticMesh* ArrowMesh, UStaticMesh* ScaleMesh, UStaticMesh* RotateRingMesh);

    void SetTargetActor(AActor* InTarget);
    AActor* GetTargetActor() const;

    void SetGizmoVisible(bool bVisible);
    bool IsGizmoVisible() const;

    void UpdateTransformFromTarget();
    void UpdateColors(EGizmoAxis HighlightAxis);

    EGizmoAxis PickAxis(
        int MouseX,
        int MouseY,
        const UCameraComponent* Camera,
        int ViewWidth,
        int ViewHeight) const;

    UStaticMeshComponent* GetAxisComponent(EGizmoAxis Axis) const;

    void SetMode(EGizmoMode InMode);
    EGizmoMode GetMode() const;

    void UpdateConstantScreenScale(
        const UCameraComponent* Camera,
        int ViewWidth,
        int ViewHeight);

private:
    bool ProjectWorldToScreen(
        const FVector& WorldPos,
        const UCameraComponent* Camera,
        int ViewWidth,
        int ViewHeight,
        float& OutX,
        float& OutY) const;

    float DistancePointToSegment2D(
        float Px, float Py,
        float Ax, float Ay,
        float Bx, float By) const;

    FVector4 LightenColor(const FVector4& Color, float T) const;

    void ApplyModeVisual();

    // 마우스 좌표 → 3D 레이 생성
    FRay BuildPickRay(
        int MouseX,
        int MouseY,
        const UCameraComponent* Camera,
        int ViewWidth,
        int ViewHeight) const;

    // 레이와 평면 교차
    bool IntersectRayPlane(
        const FRay& Ray,
        const FVector& PlanePoint,
        const FVector& PlaneNormal,
        FVector& OutHitPoint) const;

    float GetCurrentVisualScale() const;

private:
    UStaticMeshComponent* XAxisComp = nullptr;
    UStaticMeshComponent* YAxisComp = nullptr;
    UStaticMeshComponent* ZAxisComp = nullptr;

    AActor* TargetActor = nullptr;

    float AxisLength = 3.0f;
    float PickThreshold = 10.0f;
    float GizmoScale = 1.0;

    UStaticMesh* TranslateMesh = nullptr;
    UStaticMesh* ScaleMesh = nullptr;
    UStaticMesh* RotateMesh = nullptr;

    EGizmoMode CurrentMode = EGizmoMode::Translate;

    EGizmoAxis PickAxisTranslate(
        int MouseX,
		int MouseY,
        const UCameraComponent* Camera,
        int ViewWidth,
		int ViewHeight) const;

    EGizmoAxis PickAxisRotate(
		int MouseX,
        int MouseY,
        const UCameraComponent* Camera,
		int ViewWidth,
        int ViewHeight) const;

	EGizmoAxis PickAxisScale(
        int MouseX,
        int MouseY,
        const UCameraComponent* Camera,
        int ViewWidth,
		int ViewHeight) const;

    bool ProjectAxisEndToScreen(
        const FVector& Origin,
		const FVector& AxisDir,
        float Length,
		const UCameraComponent* Camera,
        int ViewWidth,
        int ViewHeight,
		float& OutOriginX,
		float& OutOriginY,
        float& OutX,
		float& OutY) const;

    float DistancePointToCircle2D(
        float Px, float Py,
        float Cx, float Cy,
		float Radius) const;

    private:
        UStaticMeshComponent* XAxisShaftComp = nullptr;
        UStaticMeshComponent* YAxisShaftComp = nullptr;
        UStaticMeshComponent* ZAxisShaftComp = nullptr;

        float RotateRingMajorRadius = 2.0f;
        float RotateRingVisualScale = 1.2f;
        float RotatePickThickness3D = 0.45f;

    USceneComponent* PivotComp = nullptr;

    float GizmoReferenceDistance = 10.0f;
    float GizmoReferenceFovDeg = 90.0f;
    float GizmoReferenceOrthoWidth = 10.0f;
    int GizmoReferenceViewportWidth = 1280;
    int GizmoReferenceViewportHeight = 720;
};