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

bool FViewportGizmoController::OnMouseButtonDown(int32 MouseX, int32 MouseY)
{
    if (bIsDragging)
    {
        return false;
    }

    if (!HitTestGizmo(MouseX, MouseY))
    {
        bIsDragging = false;
        return false;
    };

    bIsDragging = true;
    StartMousePosX = MouseX;
    StartMousePosY = MouseY;
    LastSelectedActor = ViewportSelectionController->GetSelectedActors().back();
    StartTransform = LastSelectedActor->GetRootComponent()->GetRelativeTransform();

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
        CurrentDragAxis = LastSelectedActor->GetRootComponent()->GetRelativeRotation().RotateVector(
            CurrentDragAxis);
    }

    const Geometry::FRay PickRay = Geometry::FRay::BuildRay(
        static_cast<int32>(MouseX), static_cast<int32>(MouseY),
        ViewportCamera->GetViewProjectionMatrix(), static_cast<float>(ViewportCamera->GetWidth()),
        static_cast<float>(ViewportCamera->GetHeight()));
    
    FVector PivotOrigin = StartTransform.GetLocation();

    if (GizmoType == EGizmoType::Translation && Axis == EAxis::Center)
    {
        PlaneNormal = ViewportCamera->GetForwardVector();
        InitialPlaneHit = RayPlaneIntersection(PickRay, PivotOrigin, PlaneNormal);
    }
    else if (GizmoType != EGizmoType::Rotation)
    {
        InitialDragOffset = CalculateProjectionOffset(PickRay, PivotOrigin, CurrentDragAxis);
    }
    else
    {
        // FVector HitPos = RayPlaneIntersection(PickRay, PivotOrigin, CurrentDragAxis);
        // FVector RawHitVector = HitPos - PivotOrigin;
        // RotationStartVector = RawHitVector;
        // RotationStartVector.Normalize();

        // switch (Axis)
        //{
        // case EAxis::X: // 빨간색 원 (YZ 평면) -> 평면 위의 로컬 Y축을 기준으로 삼음
        //     ReferenceAxis = FVector(0.f, -1.f, 0.f);
        //     break;
        // case EAxis::Y: // 초록색 원 (ZX 평면) -> 평면 위의 로컬 Z축을 기준으로 삼음
        //     ReferenceAxis = FVector(0.f, 0.f, -1.f);
        //     break;
        // case EAxis::Z: // 파란색 원 (XY 평면) -> 평면 위의 로컬 X축을 기준으로 삼음
        //     ReferenceAxis = FVector(-1.f, 0.f, 0.f);
        //     break;
        // }
        // if (!bIsWorldMode)
        //{
        //     ReferenceAxis =
        //     SelectedActor->GetRootComponent()->GetRelativeRotation().RotateVector(
        //         ReferenceAxis);
        // }
        // InitialProjectionT = FVector::DotProduct(RawHitVector, ReferenceAxis);

        switch (Axis)
        {
        case EAxis::X: // 빨간색 원 (YZ 평면) -> 평면 위의 로컬 Y축을 기준으로 삼음
            ReferenceAxis = FVector(0.f, -1.f, 0.f);
            break;
        case EAxis::Y: // 초록색 원 (ZX 평면) -> 평면 위의 로컬 Z축을 기준으로 삼음
            ReferenceAxis = FVector(0.f, 0.f, -1.f);
            break;
        case EAxis::Z: // 파란색 원 (XY 평면) -> 평면 위의 로컬 X축을 기준으로 삼음
            ReferenceAxis = FVector(-1.f, 0.f, 0.f);
            break;
        case EAxis::Center:
            ReferenceAxis = FVector(-1.f, -1.f, -1.f).GetSafeNormal();
        }
        if (!bIsWorldMode)
        {
            ReferenceAxis =
                LastSelectedActor->GetRootComponent()->GetRelativeRotation().RotateVector(
                ReferenceAxis);
        }
        FVector2 ScreenPosA = ProjectWorldToScreen(PivotOrigin);
        FVector2 ScreenPosB = ProjectWorldToScreen(PivotOrigin + ReferenceAxis * 100);
        ReferenceAxis2D = ScreenPosB - ScreenPosA;
        ReferenceAxis2D.Normalize();
        InitialProjectionT =
            FVector2::DotProduct(FVector2(static_cast<float>(MouseX), static_cast<float>(MouseY)) - ScreenPosA, ReferenceAxis2D);
    }

    return true;
}

void FViewportGizmoController::OnMouseButtonUp()
{
    bIsDragging = false;
    Axis = EAxis::X;
    GizmoHighlight = EGizmoHighlight::None;
    CurrentDragAxis = FVector{0.f, 0.f, 0.f};
    StartMousePosX = 0;
    StartMousePosY = 0;
    StartTransform = FTransform{};
    InitialDragOffset = 0.0f;
    
    PlaneNormal = FVector::ZeroVector;
    InitialPlaneHit = FVector::ZeroVector;
}

void FViewportGizmoController::OnMouseMove(int32 MouseX, int32 MouseY)
{
    if (bIsDragging)
    {
        UpdateDrag(MouseX, MouseY);
    }
    else
    {
        if (HitTestGizmo(MouseX, MouseY))
        {
        }
    }
}

void FViewportGizmoController::ChangeWorldMode()
{
    bIsWorldMode = !bIsWorldMode;
    CurrentDragAxis =
        LastSelectedActor->GetRootComponent()->GetRelativeRotation().RotateVector(CurrentDragAxis);
}

FMatrix FViewportGizmoController::GetMatrix() const
{
    if (LastSelectedActor)
        return LastSelectedActor->GetRootComponent()->GetRelativeMatrix();
    else
        return FMatrix::Identity;
}

bool FViewportGizmoController::HitTestGizmo(int32 MouseX, int32 MouseY)
{
    if (ViewportCamera == nullptr || ViewportClient == nullptr)
    {
        return false;
    }
    FPickResult Result = ViewportClient->PickAt(MouseX, MouseY);

    if (Result.GizmoType != EGizmoType::None)
    {
        GizmoType = Result.GizmoType;
        Axis = Result.Axis;
        switch (Axis)

        {
        case EAxis::X:
            GizmoHighlight = EGizmoHighlight::X;
            break;
        case EAxis::Y:  
            GizmoHighlight = EGizmoHighlight::Y;
            break;
        case EAxis::Z:
            GizmoHighlight = EGizmoHighlight::Z;
            break;
        case EAxis::Center:
            GizmoHighlight = EGizmoHighlight::Center;
            break;
        }
        return true;
    }
    GizmoHighlight = EGizmoHighlight::None;
    return false;
}

void FViewportGizmoController::UpdateDrag(int32 MouseX, int32 MouseY)
{
    if (GizmoType == EGizmoType::None || LastSelectedActor == nullptr)
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
    InitialDragOffset = CurrentT;
    if (bEnableSnapping && SnapValue > 0.0f)
    {
        DeltaValue = std::round(DeltaValue / SnapValue) * SnapValue;
    }
    
    if (GizmoType == EGizmoType::Translation)
    {
        if (Axis == EAxis::Center)
        {
            FVector Pivot = StartTransform.GetLocation();

            FVector CurrentHit = RayPlaneIntersection(PickRay, Pivot, PlaneNormal);

            FVector Delta = (CurrentHit - InitialPlaneHit) * TranslationDragScale;
            InitialPlaneHit = CurrentHit;

            LastFrameDelta = Delta;

            LastSelectedActor->SetLocation(
                LastSelectedActor->GetLocation() + Delta);

            TArray<AActor*>& SelectedActors{ViewportSelectionController->GetSelectedActors()};
            for (int32 idx{0}; idx < SelectedActors.size() - 1; idx++)
            {
                SelectedActors[idx]->SetLocation(
                    SelectedActors[idx]->GetLocation() + Delta);
            }

            return; 
        }

        // 기존 axis 이동
        InitialDragOffset = CurrentT;

        FVector D{CurrentDragAxis * (DeltaValue * TranslationDragScale)};
        LastFrameDelta = D;

        LastSelectedActor->SetLocation(LastSelectedActor->GetLocation() + D);

        TArray<AActor*>& SelectedActors{ViewportSelectionController->GetSelectedActors()};
        for (int32 idx{0}; idx < SelectedActors.size() - 1; idx++)
        {
            SelectedActors[idx]->SetLocation(SelectedActors[idx]->GetLocation() + D);
        }
    }
    else if (GizmoType == EGizmoType::Scaling)
    {
        FVector ScalAxis;
        switch (Axis)
        {
        case EAxis::X:
            ScalAxis = FVector{1.f, 0.f, 0.f};
            break;
        case EAxis::Y:
            ScalAxis = FVector{0.f, 1.f, 0.f};
            break;
        case EAxis::Z:
            ScalAxis = FVector{0.f, 0.f, 1.f};
            break;
        }
        FVector S{ScalAxis * DeltaValue};
        LastSelectedActor->SetScale(LastSelectedActor->GetScale() + S);

        TArray<AActor*>& SelectedActors{ViewportSelectionController->GetSelectedActors()};
        for (int32 idx{0}; idx < SelectedActors.size() - 1; idx++)
        {
            SelectedActors[idx]->SetScale(SelectedActors[idx]->GetScale() + S);
        }
    }
    else if (GizmoType == EGizmoType::Rotation)
    {
        FVector2 ScreenPosA = ProjectWorldToScreen(StartTransform.GetLocation());
        float    CurrentProjectionT = FVector2::DotProduct(
            FVector2(static_cast<float>(MouseX), static_cast<float>(MouseY)) - ScreenPosA,
            ReferenceAxis2D);
        float DeltaT = CurrentProjectionT - InitialProjectionT;
        InitialProjectionT = CurrentProjectionT;

        float RotationSensitivity = 1.0f;
        float AngleDegrees = DeltaT * RotationSensitivity;
        float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
        FQuat DeltaRotation = FQuat(CurrentDragAxis, AngleRadians);
        FQuat NewRotation = LastSelectedActor->GetRotation() * DeltaRotation;
        LastSelectedActor->SetRotion(NewRotation);

        FVector          LastActorLocation{LastSelectedActor->GetLocation()};
         TArray<AActor*>& SelectedActors{ViewportSelectionController->GetSelectedActors()};
        for (int32 idx{0}; idx < SelectedActors.size() - 1; idx++)
        {
            AActor* CurrentActor = SelectedActors[idx];
            FVector RelativeVec{CurrentActor->GetLocation()  - LastActorLocation};
            float   LengthFromLastActor{RelativeVec.Size()};
            RelativeVec.Normalize();

            CurrentActor->SetRotion(CurrentActor->GetRotation() * DeltaRotation);
            RelativeVec = DeltaRotation.RotateVector(RelativeVec);
            CurrentActor->SetLocation(LastActorLocation + RelativeVec * LengthFromLastActor);
        }
    }
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

FVector FViewportGizmoController::RayPlaneIntersection(const Geometry::FRay& Ray,
                                                       const FVector&        PlaneOrigin,
                                                       const FVector&        PlaneNormal)
{
    // Ray의 방향과 평면 법선이 수직(내적 0)이면 평행하므로 교차하지 않음
    float Denominator = FVector::DotProduct(Ray.Direction, PlaneNormal);
    if (std::abs(Denominator) < 0.0001f)
    {
        return FVector::Zero();
    }

    // t = ((PlaneOrigin - RayOrigin) dot PlaneNormal) / (RayDir dot PlaneNormal)
    float t = FVector::DotProduct(PlaneOrigin - Ray.Origin, PlaneNormal) / Denominator;

    return Ray.Origin + (Ray.Direction * t);
}

FVector2 FViewportGizmoController::ProjectWorldToScreen(const FVector& WorldPos)
{
    // 1. World -> Clip Space 변환 (4D 동차 좌표계)
    FVector4 ClipSpacePos = FVector4(WorldPos, 1.0f) * ViewportCamera->GetViewProjectionMatrix();

    // 2. Perspective Divide (W로 나누어 NDC 공간 -1 ~ 1 로 변환)
    if (std::abs(ClipSpacePos.W) > 0.00001f)
    {
        ClipSpacePos.X /= ClipSpacePos.W;
        ClipSpacePos.Y /= ClipSpacePos.W;
    }

    // 3. NDC -> Screen 픽셀 좌표계로 변환
    float ScreenX = (ClipSpacePos.X + 1.0f) * 0.5f * ViewportCamera->GetWidth();
    // Y축은 NDC에서 위가 +, 화면 픽셀은 아래가 +이므로 뒤집어줍니다.
    float ScreenY = (1.0f - ClipSpacePos.Y) * 0.5f * ViewportCamera->GetHeight();

    return FVector2(ScreenX, ScreenY);
}