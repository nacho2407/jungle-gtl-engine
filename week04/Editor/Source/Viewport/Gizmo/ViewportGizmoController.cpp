#include "ViewportGizmoController.h"
#include "Camera/ViewportCamera.h"
#include "Renderer/Types/PickId.h"
#include "Renderer/Types/PickResult.h"
#include "Viewport/EditorViewportClient.h"
#include "Viewport/Selection/ViewportSelectionController.h"
#include "Engine/Game/Actor.h"

void FViewportGizmoController::Tick(float DeltaTime)
{
    // ...
}

void FViewportGizmoController::OnMouseButtonDown(int32 MouseX, int32 MouseY)
{
    if (bIsDragging)
    {
        return;
    }
    HitTestGizmo(MouseX, MouseY);

    if (GizmoType == EGizmoType::None)
    {
        return;
    };

    bIsDragging = true;
    StartMousePosX = MouseX;
    StartMousePosY = MouseY;
    /*if (SelectedObject == nullptr)
    {
        StartTransform = SelectedObject->GetRelativeFTransform();
    }*/
    SelectedActor = ViewportSelectionController->GetSelectedActors().back();
    
    switch (Axis)
    {
    case EAxis::X:
        CurrentDragAxis = FVector{1.f, 0.f, 0.f};
        break;
    case EAxis::Y:
        CurrentDragAxis = FVector{0.f, 1.f, 0.f};
        break;
    case EAxis::Z:
        CurrentDragAxis = FVector{0.f, 0.f, 1.f};
        break;
    }
    if (!bIsWorldMode)
    {
        CurrentDragAxis =
            SelectedActor->GetRootComponent()->GetRelativeRotation().RotateVector(CurrentDragAxis);

        //CurrentDragAxis = SelectedActor->GetRelativeRotation().RotateVector(CurrentDragAxis);
    }

    const Geometry::FRay PickRay = Geometry::FRay::BuildRay(
        static_cast<int32>(MouseX), static_cast<int32>(MouseY),
        ViewportCamera->GetViewProjectionMatrix(), static_cast<float>(ViewportCamera->GetWidth()),
        static_cast<float>(ViewportCamera->GetHeight()));

    InitialDragOffset =
        CalculateProjectionOffset(PickRay, StartTransform.GetLocation(), CurrentDragAxis);
}


void FViewportGizmoController::OnMouseButtonUp()
{
    bIsDragging = false;
    GizmoType = EGizmoType::None;
    Axis = EAxis::X;
    CurrentDragAxis = FVector{0.f, 0.f, 0.f};
    StartMousePosX = 0;
    StartMousePosY = 0;
    StartTransform = FTransform{};
    InitialDragOffset = 0.0f;
}

void FViewportGizmoController::OnMouseMove(int32 MouseX, int32 MouseY)
{
    if (bIsDragging)
    {
        UpdateDrag(MouseX, MouseY);
    }
    else
    {
        // HitTestGizmo(MouseX, MouseY);
    }
}

void FViewportGizmoController::HitTestGizmo(int32 MouseX, int32 MouseY)
{
    if (ViewportCamera == nullptr || ViewportClient == nullptr)
    {
        return;
    }
    FPickResult Result = ViewportClient->PickAt(MouseX, MouseY);

    if (Result.GizmoType == EGizmoType::Translation)
    {
        GizmoType = Result.GizmoType;
        Axis = Result.Axis;
    }
    else
    {
        GizmoType = EGizmoType::None;
        Axis = EAxis::X;
    }
}

void FViewportGizmoController::UpdateDrag(int32 MouseX, int32 MouseY)
{
    if (GizmoType == EGizmoType::None || SelectedActor == nullptr)
    {
        return;
    }

    const Geometry::FRay PickRay = Geometry::FRay::BuildRay(
        static_cast<int32>(MouseX), static_cast<int32>(MouseY),
        ViewportCamera->GetViewProjectionMatrix(), static_cast<float>(ViewportCamera->GetWidth()),
        static_cast<float>(ViewportCamera->GetHeight()));

    float CurrentT =
        CalculateProjectionOffset(PickRay, StartTransform.GetLocation(), CurrentDragAxis);

    float DeltaValue = CurrentT - InitialDragOffset;

    if (bEnableSnapping && SnapValue > 0.0f)
    {
        DeltaValue = std::round(DeltaValue / SnapValue) * SnapValue;
    }

    FTransform NewTransform = StartTransform;
    FVector    NewLocation = StartTransform.GetLocation() + (CurrentDragAxis * DeltaValue);
    NewTransform.SetLocation(NewLocation);
    SelectedActor->GetRootComponent()->SetRelativeLocation(NewTransform.GetLocation());
}

float FViewportGizmoController::CalculateProjectionOffset(const Geometry::FRay& Ray,
                                                          const FVector&        AxisOrigin,
                                                          const FVector&        AxisDir)
{
    FVector w0 = Ray.Origin - AxisOrigin;

    float a = FVector::DotProduct(Ray.Direction, Ray.Direction);
    float b = FVector::DotProduct(Ray.Direction, AxisDir);
    float c = FVector::DotProduct(AxisDir, AxisDir);
    float d = FVector::DotProduct(Ray.Direction, w0);
    float e = FVector::DotProduct(AxisDir, w0);

    float Denominator = a * c - b * b;

    if (Denominator < 0.0001f)
    {
        return 0.0f;
    }
    return (a * e - b * d) / Denominator;
}
