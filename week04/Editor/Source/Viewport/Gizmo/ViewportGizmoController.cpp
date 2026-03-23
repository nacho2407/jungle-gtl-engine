#include "ViewportGizmoController.h"
#include "Camera/ViewportCamera.h"

void FViewportGizmoController::Tick(float DeltaTime)
{
    // ...
}

void FViewportGizmoController::OnMouseButtonDown(const FVector2& MousePos)
{
    if (bIsDragging)
    {
        return;
    }
    ActiveAxisIndex = HitTestGizmo(MousePos);

    if (ActiveAxisIndex != EGizmoAxis::None)
    {
        bIsDragging = true;
        StartMousePos = MousePos;
        StartTransform = SelectedObject->GetRelativeFTransform();

        switch (ActiveAxisIndex)
        {
        case EGizmoAxis::X:
            CurrentDragAxis = FVector{1.f, 0.f, 0.f};
            break;
        case EGizmoAxis::Y:
            CurrentDragAxis = FVector{0.f, 1.f, 0.f};
            break;
        case EGizmoAxis::Z:
            CurrentDragAxis = FVector{0.f, 0.f, 1.f};
            break;
        case EGizmoAxis::XY:
            CurrentDragAxis = FVector{1.f, 1.f, 0.f};
            break;
        case EGizmoAxis::YZ:
            CurrentDragAxis = FVector{0.f, 1.f, 1.f};
            break;
        case EGizmoAxis::ZX:
            CurrentDragAxis = FVector{1.f, 0.f, 1.f};
            break;
        case EGizmoAxis::XYZ:
            CurrentDragAxis = FVector{1.f, 1.f, 1.f};
            break;
        }
        (void)CurrentDragAxis.Normalize();

        if (!bIsWorldMode)
        {
            CurrentDragAxis = SelectedObject->GetRelativeRotation().RotateVector(CurrentDragAxis);
        }

        const Geometry::FRay PickRay =
            Geometry::FRay::BuildRay(static_cast<int32>(MousePos.X), static_cast<int32>(MousePos.Y),
                                     ViewportCamera->GetViewProjectionMatrix(),
                                     static_cast<float>(ViewportCamera->GetWidth()),
                                     static_cast<float>(ViewportCamera->GetHeight()));

        InitialDragOffset =
            CalculateProjectionOffset(PickRay, StartTransform.GetLocation(), CurrentDragAxis);
    }
}

void FViewportGizmoController::OnMouseButtonUp()
{
    bIsDragging = false;
    ActiveAxisIndex = EGizmoAxis::None;
    CurrentDragAxis = FVector{0.f, 0.f, 0.f};
    StartMousePos = FVector2{0.f, 0.f};
    StartTransform = FTransform{};
    InitialDragOffset = 0.0f;
}

void FViewportGizmoController::OnMouseMove(const FVector2& MousePos)
{
    if (bIsDragging)
    {
        UpdateDrag(MousePos);
    }
    else
    {
        ActiveAxisIndex = HitTestGizmo(MousePos);
    }
}

EGizmoAxis FViewportGizmoController::HitTestGizmo(const FVector2& MousePos)
{
    if (ViewportCamera == nullptr || SceneRenderData == nullptr)
    {
        return EGizmoAxis::None;
    }

    return EGizmoAxis::None;
}

void FViewportGizmoController::UpdateDrag(const FVector2& MousePos)
{
    if (ActiveAxisIndex == EGizmoAxis::None || SelectedObject == nullptr)
    {
        return;
    }

    const Geometry::FRay PickRay = Geometry::FRay::BuildRay(
        static_cast<int32>(MousePos.X), static_cast<int32>(MousePos.Y),
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
    SelectedObject->SetRelativeLocation(NewTransform.GetLocation());
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
