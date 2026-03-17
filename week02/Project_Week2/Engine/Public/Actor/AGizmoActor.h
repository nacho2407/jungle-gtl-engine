#pragma once

#include "Actor/AActor.h"
#include "Component/EGizmoAxis.h"
#include "Math/FVector4.h"
#include "Resource/BuiltInMeshFactory.h"
#include "Component/UCameraComponent.h"
#include "Component/UStaticMeshComponent.h"
#include "Component/EGizmoMode.h"

//class UStaticMesh;
//class UStaticMeshComponent;
//class UCameraComponent;

class AGizmoActor : public AActor 
{
DECLARE_UClass(AGizmoActor, AActor)
public:
    AGizmoActor();
    virtual ~AGizmoActor() override; // 왜 가상함수로 했을까

public:
    void Initialize(UStaticMesh* ArrowMesh, UStaticMesh* CubeMesh, UStaticMesh* TorusMesh);

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

private:
    UStaticMeshComponent* XAxisComp = nullptr;
    UStaticMeshComponent* YAxisComp = nullptr;
    UStaticMeshComponent* ZAxisComp = nullptr;

    AActor* TargetActor = nullptr;

    float AxisLength = 3.0f;
    float PickThreshold = 10.0f;
    float GizmoScale = 0.5f;

    UStaticMesh* TranslateMesh = nullptr;
    UStaticMesh* ScaleMesh = nullptr;
    UStaticMesh* RotateMesh = nullptr;

    EGizmoMode CurrentMode = EGizmoMode::Translate;
};