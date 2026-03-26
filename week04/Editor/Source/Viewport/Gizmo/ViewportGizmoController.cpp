#include "ViewportGizmoController.h"
#include "Camera/ViewportCamera.h"
#include "Renderer/Types/PickId.h"
#include "Renderer/Types/PickResult.h"
#include "Viewport/EditorViewportClient.h"
#include "Viewport/Selection/ViewportSelectionController.h"
#include "Engine/Game/Actor.h"

void FViewportGizmoController::Tick(float DeltaTime)
{
    //  Do nothing
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

    //  다중 선택 대비
    InitialActorLocations.clear();
    InitialActorScales.clear();
    InitialActorOffsets.clear();

    TArray<AActor*>& SelectedActors{ViewportSelectionController->GetSelectedActors()};
    FVector          Pivot = StartTransform.GetLocation();

    for (AActor* Actor : SelectedActors)
    {
        InitialActorLocations.push_back(Actor->GetLocation());
        InitialActorScales.push_back(Actor->GetScale());
        InitialActorOffsets.push_back(Actor->GetLocation() - Pivot); // 핵심
    }

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
        PrevSnappedCenterDelta = FVector::ZeroVector;
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
        // 수정: 회전은 screen projection이 아니라
        // "회전축(CurrentDragAxis)을 법선으로 하는 평면" 위의 시작 방향 벡터를 저장해서 처리한다.
        // 이렇게 해야 시계/반시계 방향이 올바르게 계산된다.

        PrevSnappedOffset = 0.0f;

        FVector HitPoint = RayPlaneIntersection(PickRay, PivotOrigin, CurrentDragAxis);
        FVector StartDir = HitPoint - PivotOrigin;

        // 수정: ray-plane 결과가 불안정한 경우를 대비
        if (StartDir.IsNearlyZero())
        {
            // 카메라 기준으로 회전축에 수직인 보조 벡터를 하나 만든다.
            FVector Fallback =
                FVector::CrossProduct(CurrentDragAxis, ViewportCamera->GetForwardVector());
            if (Fallback.IsNearlyZero())
            {
                Fallback = FVector::CrossProduct(CurrentDragAxis, FVector::UpVector);
            }
            InitialHitDir = Fallback.GetSafeNormal();
        }
        else
        {
            InitialHitDir = StartDir.GetSafeNormal();
        }
    }
    else
    {
        DragStartOffset = CalculateProjectionOffset(PickRay, PivotOrigin, CurrentDragAxis);
        PrevSnappedOffset = 0.0f; // 수정: 누적 기준 초기화
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
    LastFrameDelta = FVector::ZeroVector;
    PrevSnappedCenterDelta = FVector::ZeroVector;

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
        HitTestGizmo(MouseX, MouseY);
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
            FVector Pivot = StartTransform.GetLocation();
            FVector CurrentHit = RayPlaneIntersection(PickRay, Pivot, PlaneNormal);

            // 수정: 전체 이동량 기준
            FVector RawTotalDelta = CurrentHit - InitialPlaneHit;

            FVector SnappedTotalDelta = RawTotalDelta;

            // 수정: 축별 누적 snapping
            if (bEnableTranslationSnap && TranslationSnapValue > 0.0f)
            {
                SnappedTotalDelta.X =
                    std::round(RawTotalDelta.X / TranslationSnapValue) * TranslationSnapValue;
                SnappedTotalDelta.Y =
                    std::round(RawTotalDelta.Y / TranslationSnapValue) * TranslationSnapValue;
                SnappedTotalDelta.Z =
                    std::round(RawTotalDelta.Z / TranslationSnapValue) * TranslationSnapValue;
            }

            // 카메라 동기화를 위해 프레임 delta를 저장
            const FVector FrameDelta =
                (SnappedTotalDelta - PrevSnappedCenterDelta) * TranslationDragScale;
            PrevSnappedCenterDelta = SnappedTotalDelta;
            LastFrameDelta = FrameDelta;

            const FVector AppliedTotalDelta = SnappedTotalDelta * TranslationDragScale;

            LastSelectedActor->SetLocation(StartTransform.GetLocation() + AppliedTotalDelta);

            TArray<AActor*>& SelectedActors{ViewportSelectionController->GetSelectedActors()};
            const int32 SelectedCount = static_cast<int32>(SelectedActors.size());

            for (int32 i = 0; i < SelectedCount; i++)
            {
                SelectedActors[i]->SetLocation(
                    InitialActorLocations[i] + AppliedTotalDelta
                );
            }
            
            return;
        }

        float CurrentT =
            CalculateProjectionOffset(PickRay, StartTransform.GetLocation(), CurrentDragAxis);

        // 수정: "프레임 delta"가 아니라 "전체 이동량" 기준
        float RawTotalOffset = CurrentT - DragStartOffset;

        float SnappedTotalOffset = RawTotalOffset;

        // 수정: 누적 기준 snapping
        if (bEnableTranslationSnap && TranslationSnapValue > 0.0f)
        {
            SnappedTotalOffset =
                std::round(RawTotalOffset / TranslationSnapValue) * TranslationSnapValue;
        }

        // 수정: 이번 프레임 delta 계산
        float FrameDelta = SnappedTotalOffset - PrevSnappedOffset;
        PrevSnappedOffset = SnappedTotalOffset;

        // 이동 적용
        FVector D = CurrentDragAxis * (FrameDelta * TranslationDragScale);
        LastFrameDelta = D;

        LastSelectedActor->SetLocation(LastSelectedActor->GetLocation() + D);

        TArray<AActor*>& SelectedActors{ViewportSelectionController->GetSelectedActors()};
        const int32 SelectedCount = static_cast<int32>(SelectedActors.size());

        for (int32 idx{0}; idx < SelectedCount - 1; idx++)
        {
            SelectedActors[idx]->SetLocation(SelectedActors[idx]->GetLocation() + D);
        }
    }
    else if (GizmoType == EGizmoType::Scaling)
    {
        if (Axis == EAxis::Center)
        {
            FVector2 ScreenPosA = ProjectWorldToScreen(StartTransform.GetLocation());

            FVector2 MouseDelta = FVector2(static_cast<float>(MouseX) - ScreenPosA.X,
                                           static_cast<float>(MouseY) - ScreenPosA.Y);

            FVector2 Dir(1.f, -1.f);
            Dir.Normalize();

            //  누적 방식
            float RawTotal = FVector2::DotProduct(MouseDelta, Dir) - InitialProjectionT;

            float SnappedTotal = RawTotal;

            if (bEnableScaleSnap && ScaleSnapValue > 0.0f)
            {
                SnappedTotal = std::round(RawTotal / ScaleSnapValue) * ScaleSnapValue;
            }

            float FrameDelta = SnappedTotal - PrevSnappedOffset;
            PrevSnappedOffset = SnappedTotal;

            float ScaleSensitivity = 0.01f;
            float ScaleDelta = FrameDelta * ScaleSensitivity;

            float ScaleRatio = 1.0f + (SnappedTotal * ScaleSensitivity);

            TArray<AActor*>& SelectedActors{ViewportSelectionController->GetSelectedActors()};
            const int32      SelectedCount = static_cast<int32>(SelectedActors.size());

            for (int32 i = 0; i < SelectedCount; i++)
            {
                FVector NewScale = InitialActorScales[i] * ScaleRatio;

                NewScale = FVector(std::max(0.01f, NewScale.X), std::max(0.01f, NewScale.Y),
                                   std::max(0.01f, NewScale.Z));

                SelectedActors[i]->SetScale(NewScale);

                //  위치도 같이 변해야 함
                FVector NewOffset = InitialActorOffsets[i] * ScaleRatio;
                SelectedActors[i]->SetLocation(StartTransform.GetLocation() + NewOffset);
            }
            return;
        }

        float CurrentT =
            CalculateProjectionOffset(PickRay, StartTransform.GetLocation(), CurrentDragAxis);

        float RawTotal = CurrentT - DragStartOffset;

        float SnappedTotal = RawTotal;

        if (bEnableScaleSnap && ScaleSnapValue > 0.0f)
        {
            SnappedTotal = std::round(RawTotal / ScaleSnapValue) * ScaleSnapValue;
        }

        float FrameDelta = SnappedTotal - PrevSnappedOffset;
        PrevSnappedOffset = SnappedTotal;

        FVector ScaleAxis = FVector::ZeroVector;
        switch (Axis)
        {
        case EAxis::X:
            ScaleAxis = FVector{1.f, 0.f, 0.f};
            break;
        case EAxis::Y:
            ScaleAxis = FVector{0.f, 1.f, 0.f};
            break;
        case EAxis::Z:
            ScaleAxis = FVector{0.f, 0.f, 1.f};
            break;
        default:
            return;
        }

        // 수정: 다중 선택 시에도 "시작 scale / 시작 offset" 기준으로 처리
        // 축 스케일은 선택 pivot(StartTransform.GetLocation()) 기준으로
        // 해당 축 방향으로만 위치/스케일을 늘리거나 줄임
        TArray<AActor*>& SelectedActors{ViewportSelectionController->GetSelectedActors()};
        const int32      SelectedCount = static_cast<int32>(SelectedActors.size());

        for (int32 i = 0; i < SelectedCount; i++)
        {
            FVector NewScale = InitialActorScales[i] + (ScaleAxis * SnappedTotal);
            NewScale = FVector(std::max(0.01f, NewScale.X), std::max(0.01f, NewScale.Y),
                               std::max(0.01f, NewScale.Z));
            SelectedActors[i]->SetScale(NewScale);

            FVector NewOffset = InitialActorOffsets[i];

            if (Axis == EAxis::X)
            {
                const float BaseX = InitialActorOffsets[i].X;
                const float BaseScaleX = std::max(0.01f, InitialActorScales[i].X);
                const float NewScaleX = std::max(0.01f, NewScale.X);
                NewOffset.X = BaseX * (NewScaleX / BaseScaleX);
            }
            else if (Axis == EAxis::Y)
            {
                const float BaseY = InitialActorOffsets[i].Y;
                const float BaseScaleY = std::max(0.01f, InitialActorScales[i].Y);
                const float NewScaleY = std::max(0.01f, NewScale.Y);
                NewOffset.Y = BaseY * (NewScaleY / BaseScaleY);
            }
            else if (Axis == EAxis::Z)
            {
                const float BaseZ = InitialActorOffsets[i].Z;
                const float BaseScaleZ = std::max(0.01f, InitialActorScales[i].Z);
                const float NewScaleZ = std::max(0.01f, NewScale.Z);
                NewOffset.Z = BaseZ * (NewScaleZ / BaseScaleZ);
            }

            SelectedActors[i]->SetLocation(StartTransform.GetLocation() + NewOffset);
        }

        return;
    }
    else if (GizmoType == EGizmoType::Rotation)
    {
        // 수정: 회전은 "회전축 평면 위의 시작 벡터 vs 현재 벡터"의 signed angle로 계산한다.
        // 기존처럼 2D projection scalar만 보면 방향 정보가 깨져서 한 방향으로만 돌거나 반대로 도는
        // 문제가 생긴다.

        FVector Pivot = StartTransform.GetLocation();

        FVector HitPoint = RayPlaneIntersection(PickRay, Pivot, CurrentDragAxis);
        FVector CurrentDir = HitPoint - Pivot;

        if (CurrentDir.IsNearlyZero() || InitialHitDir.IsNearlyZero())
        {
            return;
        }

        CurrentDir = CurrentDir.GetSafeNormal();

        const float DotValue =
            FMath::Clamp(FVector::DotProduct(InitialHitDir, CurrentDir), -1.0f, 1.0f);
        const FVector CrossValue = FVector::CrossProduct(InitialHitDir, CurrentDir);

        // 수정: atan2(sin, cos) 형태로 signed angle 계산
        float SignedAngleRad =
            std::atan2(FVector::DotProduct(CurrentDragAxis, CrossValue), DotValue);

        float SignedAngleDeg = FMath::RadiansToDegrees(SignedAngleRad);

        // 수정: 누적 스냅은 "전체 각도" 기준으로 적용
        float SnappedTotalDeg = SignedAngleDeg;
        if (bEnableRotationSnap && RotationSnapValue > 0.0f)
        {
            SnappedTotalDeg = std::round(SignedAngleDeg / RotationSnapValue) * RotationSnapValue;
        }

        float FrameDeltaDeg = SnappedTotalDeg - PrevSnappedOffset;
        PrevSnappedOffset = SnappedTotalDeg;

        if (FMath::IsNearlyZero(FrameDeltaDeg))
        {
            return;
        }

        float FrameDeltaRad = FMath::DegreesToRadians(FrameDeltaDeg);
        FQuat DeltaRotation(CurrentDragAxis, FrameDeltaRad);
        DeltaRotation.Normalize();

        // 수정: 기준 actor 회전
        FQuat NewRotation = LastSelectedActor->GetRotation() * DeltaRotation;
        NewRotation.Normalize();
        LastSelectedActor->SetRotion(NewRotation);

        // 수정: 다중 선택도 같은 delta rotation 적용
        TArray<AActor*>& SelectedActors{ViewportSelectionController->GetSelectedActors()};
        const int32      SelectedCount = static_cast<int32>(SelectedActors.size());

        FVector PivotLocation = LastSelectedActor->GetLocation();

        for (int32 idx{0}; idx < SelectedCount - 1; idx++)
        {
            AActor* Actor = SelectedActors[idx];
            if (Actor == nullptr)
            {
                continue;
            }

            FVector Offset = Actor->GetLocation() - PivotLocation;
            Offset = DeltaRotation.RotateVector(Offset);

            FQuat ActorRotation = Actor->GetRotation() * DeltaRotation;
            ActorRotation.Normalize();

            Actor->SetRotion(ActorRotation);
            Actor->SetLocation(PivotLocation + Offset);
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