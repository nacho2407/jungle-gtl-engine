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
    case EAxis::Center:
        CurrentDragAxis = FVector::ZeroVector; // 수정: Center는 축 이동이 아니라 별도 분기 처리
        break;
    }
    if (!bIsWorldMode && Axis != EAxis::Center) // 수정: Center에는 축 회전 적용하지 않음
    {
        CurrentDragAxis = LastSelectedActor->GetRootComponent()->GetRelativeRotation().RotateVector(
            CurrentDragAxis);
    }

    const Geometry::FRay PickRay = Geometry::FRay::BuildRay(
        static_cast<int32>(MouseX), static_cast<int32>(MouseY),
        ViewportCamera->GetViewProjectionMatrix(), static_cast<float>(ViewportCamera->GetWidth()),
        static_cast<float>(ViewportCamera->GetHeight()));
    FVector PivotOrigin = StartTransform.GetLocation();

    // 수정: Center Translation / Center Scaling / Rotation / 일반 Axis 를 명확히 분리
    if (GizmoType == EGizmoType::Translation && Axis == EAxis::Center)
    {
        PlaneNormal = ViewportCamera->GetForwardVector();
        InitialPlaneHit = RayPlaneIntersection(PickRay, PivotOrigin, PlaneNormal);
    }
    else if (GizmoType == EGizmoType::Scaling && Axis == EAxis::Center)
    {
        FVector2 ScreenPosA = ProjectWorldToScreen(PivotOrigin);
        FVector2 Dir(1.f, -1.f);
        Dir.Normalize();

        InitialProjectionT = FVector2::DotProduct(
            FVector2(static_cast<float>(MouseX), static_cast<float>(MouseY)) - ScreenPosA,
            Dir);
    }
    else if (GizmoType == EGizmoType::Rotation)
    {
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
            break;
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
    else
    {
        InitialDragOffset = CalculateProjectionOffset(PickRay, PivotOrigin, CurrentDragAxis);
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

    PlaneNormal = FVector::ZeroVector;      // 수정: Center Translation 상태 초기화
    InitialPlaneHit = FVector::ZeroVector;  // 수정: Center Translation 상태 초기화
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

    // 수정: null 안전성 추가
    if (LastSelectedActor == nullptr || LastSelectedActor->GetRootComponent() == nullptr)
    {
        return;
    }

    // 수정: Center는 별도 로직이라 축 회전하지 않음
    if (Axis != EAxis::Center)
    {
        CurrentDragAxis =
            LastSelectedActor->GetRootComponent()->GetRelativeRotation().RotateVector(CurrentDragAxis);
    }
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

    FTransform NewTransform = StartTransform;

    if (GizmoType == EGizmoType::Translation)
    {
        if (Axis == EAxis::Center)
        {
            // 수정: Center Translation은 axis projection이 아니라 camera-facing plane 사용
            FVector Pivot = StartTransform.GetLocation();
            FVector CurrentHit = RayPlaneIntersection(PickRay, Pivot, PlaneNormal);

            FVector Delta = (CurrentHit - InitialPlaneHit) * TranslationDragScale;
            InitialPlaneHit = CurrentHit;

            // 수정: Center Translation에도 snapping 적용
            if (bEnableTranslationSnap && TranslationSnapValue > 0.0f)
            {
                Delta.X = std::round(Delta.X / TranslationSnapValue) * TranslationSnapValue;
                Delta.Y = std::round(Delta.Y / TranslationSnapValue) * TranslationSnapValue;
                Delta.Z = std::round(Delta.Z / TranslationSnapValue) * TranslationSnapValue;
            }

            LastFrameDelta = Delta;
            LastSelectedActor->SetLocation(LastSelectedActor->GetLocation() + Delta);

            TArray<AActor*>& SelectedActors{ViewportSelectionController->GetSelectedActors()};
            const int32 SelectedCount = static_cast<int32>(SelectedActors.size()); // 수정: 안전한 반복
            for (int32 idx{0}; idx < SelectedCount - 1; idx++)
            {
                SelectedActors[idx]->SetLocation(SelectedActors[idx]->GetLocation() + Delta);
            }
            return;
        }

        // 수정: axis translation에서만 projection 계산
        float CurrentT =
            CalculateProjectionOffset(PickRay, StartTransform.GetLocation(), CurrentDragAxis);

        float DeltaValue = CurrentT - InitialDragOffset;
        InitialDragOffset = CurrentT;
        if (bEnableTranslationSnap && TranslationSnapValue > 0.0f)
        {
            DeltaValue = std::round(DeltaValue / TranslationSnapValue) * TranslationSnapValue;
        }

        FVector D{CurrentDragAxis * (DeltaValue * TranslationDragScale)};
        LastFrameDelta = D;
        LastSelectedActor->SetLocation(LastSelectedActor->GetLocation() + D);

        TArray<AActor*>& SelectedActors{ViewportSelectionController->GetSelectedActors()};
        const int32 SelectedCount = static_cast<int32>(SelectedActors.size()); // 수정: 안전한 반복
        for (int32 idx{0}; idx < SelectedCount - 1; idx++)
        {
            SelectedActors[idx]->SetLocation(SelectedActors[idx]->GetLocation() + D);
        }
        return;
    }
    else if (GizmoType == EGizmoType::Scaling)
    {
        if (Axis == EAxis::Center)
        {
            FVector2 ScreenPosA = ProjectWorldToScreen(StartTransform.GetLocation());

            FVector2 MouseDelta = FVector2(
                static_cast<float>(MouseX) - ScreenPosA.X,
                static_cast<float>(MouseY) - ScreenPosA.Y);

            FVector2 Dir = FVector2(1.f, -1.f);
            Dir.Normalize();

            float CurrentProjectionT = FVector2::DotProduct(MouseDelta, Dir);
            float DeltaT = CurrentProjectionT - InitialProjectionT;
            InitialProjectionT = CurrentProjectionT;

            float ScaleSensitivity = 0.01f;
            float ScaleDelta = DeltaT * ScaleSensitivity;

            if (bEnableScaleSnap && ScaleSnapValue > 0.0f)
            {
                ScaleDelta = std::round(ScaleDelta / ScaleSnapValue) * ScaleSnapValue;
            }

            FVector NewScale = LastSelectedActor->GetScale() +
                               FVector(ScaleDelta, ScaleDelta, ScaleDelta);

            NewScale = FVector(
                std::max(0.01f, NewScale.X),
                std::max(0.01f, NewScale.Y),
                std::max(0.01f, NewScale.Z)); // 수정: 음수/0 scale 방지

            LastSelectedActor->SetScale(NewScale);

            TArray<AActor*>& SelectedActors{ViewportSelectionController->GetSelectedActors()};
            const int32 SelectedCount = static_cast<int32>(SelectedActors.size()); // 수정: 안전한 반복
            for (int32 idx{0}; idx < SelectedCount - 1; idx++)
            {
                FVector OtherScale = SelectedActors[idx]->GetScale() +
                                     FVector(ScaleDelta, ScaleDelta, ScaleDelta);

                OtherScale = FVector(
                    std::max(0.01f, OtherScale.X),
                    std::max(0.01f, OtherScale.Y),
                    std::max(0.01f, OtherScale.Z)); // 수정: 음수/0 scale 방지

                SelectedActors[idx]->SetScale(OtherScale);
            }
            return;
        }

        // 수정: axis scaling에서만 projection 계산
        float CurrentT =
            CalculateProjectionOffset(PickRay, StartTransform.GetLocation(), CurrentDragAxis);

        float DeltaValue = CurrentT - InitialDragOffset;
        InitialDragOffset = CurrentT;

        float ScaleDelta = DeltaValue; // 수정: axis scaling에도 별도 scale snap 적용
        if (bEnableScaleSnap && ScaleSnapValue > 0.0f)
        {
            ScaleDelta = std::round(ScaleDelta / ScaleSnapValue) * ScaleSnapValue;
        }

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
        case EAxis::Center:
            ScalAxis = FVector::ZeroVector;
            break;
        }

        FVector S{ScalAxis * ScaleDelta};

        FVector NewScale = LastSelectedActor->GetScale() + S;
        NewScale = FVector(
            std::max(0.01f, NewScale.X),
            std::max(0.01f, NewScale.Y),
            std::max(0.01f, NewScale.Z)); // 수정: axis scaling에도 clamp 적용
        LastSelectedActor->SetScale(NewScale);

        TArray<AActor*>& SelectedActors{ViewportSelectionController->GetSelectedActors()};
        const int32 SelectedCount = static_cast<int32>(SelectedActors.size()); // 수정: 안전한 반복
        for (int32 idx{0}; idx < SelectedCount - 1; idx++)
        {
            FVector OtherScale = SelectedActors[idx]->GetScale() + S;
            OtherScale = FVector(
                std::max(0.01f, OtherScale.X),
                std::max(0.01f, OtherScale.Y),
                std::max(0.01f, OtherScale.Z)); // 수정: axis scaling에도 clamp 적용
            SelectedActors[idx]->SetScale(OtherScale);
        }
        return;
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
        if (bEnableRotationSnap && RotationSnapValue > 0.f) // 수정: 잘못된 조건 변수 수정
        {
            AngleDegrees = std::round(AngleDegrees / RotationSnapValue) * RotationSnapValue;
        }
        float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
        FQuat DeltaRotation = FQuat(CurrentDragAxis, AngleRadians);
        FQuat NewRotation = LastSelectedActor->GetRotation() * DeltaRotation;
        LastSelectedActor->SetRotion(NewRotation);

        FVector          LastActorLocation{LastSelectedActor->GetLocation()};
        TArray<AActor*>& SelectedActors{ViewportSelectionController->GetSelectedActors()};
        const int32 SelectedCount = static_cast<int32>(SelectedActors.size()); // 수정: 안전한 반복
        for (int32 idx{0}; idx < SelectedCount - 1; idx++)
        {
            AActor* CurrentActor = SelectedActors[idx];
            FVector RelativeVec{CurrentActor->GetLocation() - LastActorLocation};
            float   LengthFromLastActor{RelativeVec.Size()};
            RelativeVec.Normalize();

            CurrentActor->SetRotion(CurrentActor->GetRotation() * DeltaRotation);
            RelativeVec = DeltaRotation.RotateVector(RelativeVec);
            CurrentActor->SetLocation(LastActorLocation + RelativeVec * LengthFromLastActor);
        }
        return;
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