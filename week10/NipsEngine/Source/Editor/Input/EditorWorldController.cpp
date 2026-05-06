#include "EditorWorldController.h"
#include "Editor/Selection/SelectionManager.h"
#include "Camera/ViewportCamera.h"
#include "Editor/Settings/EditorSettings.h"
#include "Engine/Component/GizmoComponent.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Level.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Collision/RayCollision/RayCollision.h"
#include "Math/Utils.h"

#include <algorithm>
#include <cmath>
#include <windows.h>

void FEditorWorldController::SetCamera(FViewportCamera* InCamera)
{
    if (!InCamera)
        return;
    Camera = InCamera;
    ResetTargetFromCamera();
}

void FEditorWorldController::SetCamera(FViewportCamera& InCamera)
{
    Camera = &InCamera;
    ResetTargetFromCamera();
}

void FEditorWorldController::ResetTargetFromCamera()
{
    if (!Camera)
    {
        return;
    }

    TargetLocation = Camera->GetLocation();
    TargetRotation = Camera->GetRotation();
    bTargetLocationInitialized = true;
    bTargetRotationInitialized = true;
    SeedYawPitchFromCamera();
}

void FEditorWorldController::Tick(float InDeltaTime)
{
    DeltaTime = InDeltaTime;
    if (!Camera)
        return;

    if (!bTargetLocationInitialized)
    {
        TargetLocation = Camera->GetLocation();
        bTargetLocationInitialized = true;
    }
    if (!bTargetRotationInitialized)
    {
        TargetRotation = Camera->GetRotation();
        bTargetRotationInitialized = true;
    }

    const FEditorSettings& Settings = FEditorSettings::Get();
    if (!Settings.bEnableCameraSmoothing)
    {
        Camera->SetLocation(TargetLocation);
        Camera->SetRotation(TargetRotation);
        return;
    }

    const float MoveAlpha = MathUtil::Clamp(DeltaTime * std::max(0.1f, Settings.CameraMoveSmoothSpeed), 0.0f, 1.0f);
    const float RotateAlpha = MathUtil::Clamp(DeltaTime * std::max(0.1f, Settings.CameraRotateSmoothSpeed), 0.0f, 1.0f);
    Camera->SetLocation(FVector::Lerp(Camera->GetLocation(), TargetLocation, MoveAlpha));
    Camera->SetRotation(FQuat::Slerp(Camera->GetRotation(), TargetRotation, RotateAlpha));
}

// X/Y parameters for mouse events are viewport-local pixel coordinates.
// DeltaX/DeltaY parameters are raw mouse movement deltas (pixels since last frame).
void FEditorWorldController::OnMouseMoveAbsolute(float X, float Y)
{
    if (!Gizmo || !Gizmo->IsVisible() || !Camera)
        return;

    // Update gizmo hover highlight each frame
    FRay       Ray = Camera->DeprojectScreenToWorld(X, Y, ViewportWidth, ViewportHeight);
    FHitResult HitResult;
    Gizmo->RaycastMesh(Ray, HitResult);
}

void FEditorWorldController::OnLeftMouseClick(float X, float Y)
{
    ClearPendingSelectionPress();

    // Try gizmo handle first. Actor selection is committed on release so small
    // camera/gizmo drags do not accidentally click-select.
    if (!Camera)
        return;

    FRay       Ray = Camera->DeprojectScreenToWorld(X, Y, ViewportWidth, ViewportHeight);
    FHitResult HitResult{};

    if (Gizmo && Gizmo->IsVisible() && FRayCollision::RaycastComponent(Gizmo, Ray, HitResult))
    {
        Gizmo->SetPressedOnHandle(true);
        return;
    }
    else if (Gizmo)
    {
        Gizmo->SetPressedOnHandle(false);
	}

    bHasPendingSelectionPress = true;
    PendingSelectionPressX = X;
    PendingSelectionPressY = Y;
}

void FEditorWorldController::SelectActorAt(float X, float Y)
{
    if (!World || !SelectionManager)
        return;

    AActor* BestActor = nullptr;
    if (SelectionPickResolver)
    {
        SelectionPickResolver(X, Y, BestActor);
    }
    else
    {
        FRay       Ray = Camera->DeprojectScreenToWorld(X, Y, ViewportWidth, ViewportHeight);
        FHitResult HitResult{};
        float   ClosestDist = FLT_MAX;

        FWorldSpatialIndex::FPrimitiveRayQueryScratch RayQueryScratch;
        TArray<UPrimitiveComponent*> CandidatePrimitives;
        TArray<float>                CandidateTs;
        World->GetSpatialIndex().RayQueryPrimitives(Ray, CandidatePrimitives, CandidateTs, RayQueryScratch);

        for (int32 CandidateIndex = 0; CandidateIndex < static_cast<int32>(CandidatePrimitives.size()); ++CandidateIndex)
        {
            if (CandidateTs[CandidateIndex] > ClosestDist)
            {
                break;
            }

            UPrimitiveComponent* PrimitiveComp = CandidatePrimitives[CandidateIndex];
            AActor*              Actor = (PrimitiveComp != nullptr) ? PrimitiveComp->GetOwner() : nullptr;
            if (Actor == nullptr || Actor->GetRootComponent() == nullptr)
            {
                continue;
            }

            HitResult = {};
            if (PrimitiveComp->Raycast(Ray, HitResult) && HitResult.Distance < ClosestDist)
            {
                ClosestDist = HitResult.Distance;
                BestActor = Actor;
            }
        }
    }

	InputSystem& IS = InputSystem::Get();

    bool bCtrl = IS.GetKey(VK_CONTROL);
    bool bShift = IS.GetKey(VK_SHIFT);
    if (!BestActor)
    {
        if (!bCtrl && !bShift)
            SelectionManager->ClearSelection();
    }
    else
    {
        if (bCtrl)
            SelectionManager->ToggleSelect(BestActor);
        else if (bShift)
            SelectionManager->AddSelect(BestActor);
        else
            SelectionManager->Select(BestActor);
    }
}

void FEditorWorldController::OnLeftMouseDrag(float X, float Y)
{
    ClearPendingSelectionPress();

    if (!Gizmo || !Gizmo->IsVisible() || !Camera)
        return;

    FRay Ray = Camera->DeprojectScreenToWorld(X, Y, ViewportWidth, ViewportHeight);

    // First frame of drag: arm the gizmo hold
    if (Gizmo->IsPressedOnHandle() && !Gizmo->IsHolding())
        Gizmo->SetHolding(true);

    if (Gizmo->IsHolding())
        Gizmo->UpdateDrag(Ray);
}

void FEditorWorldController::OnLeftMouseDragEnd(float X, float Y)
{
    (void)X;
    (void)Y;
    ClearPendingSelectionPress();
    if (Gizmo)
        Gizmo->DragEnd();
}

void FEditorWorldController::OnLeftMouseButtonUp(float X, float Y)
{
    (void)X;
    (void)Y;
    if (bHasPendingSelectionPress && Camera)
    {
        SelectActorAt(PendingSelectionPressX, PendingSelectionPressY);
    }
    ClearPendingSelectionPress();

    // LMB released without reaching drag threshold - disarm gizmo
    if (Gizmo)
        Gizmo->SetPressedOnHandle(false);
}

void FEditorWorldController::OnRightMouseClick(float DeltaX, float DeltaY)
{
    // Seed Yaw/Pitch from current camera orientation so the first drag doesn't snap
    if (!Camera)
        return;
    SeedYawPitchFromCamera();
}

void FEditorWorldController::OnRightMouseDrag(float DeltaX, float DeltaY)
{
    if (!Camera)
        return;

    // 수식 키(Ctrl, Alt, Shift)가 눌려 있으면 뷰포트 이동을 차단합니다.
    const InputSystem& IS = InputSystem::Get();
    if (IS.GetKey(VK_CONTROL) || IS.GetKey(VK_MENU) || IS.GetKey(VK_SHIFT))
        return;

    if (Camera->IsOrthographic())
    {
        // Pan: scale movement proportionally to current ortho zoom level
        const float     PanScale = Camera->GetOrthoHeight() * 0.001f * FEditorSettings::Get().CameraPanSpeedScale;
        const FVector   Right    = Camera->GetEffectiveRight();
        const FVector   Up       = Camera->GetEffectiveUp();
        TargetLocation += Right * (-DeltaX * PanScale) + Up * (DeltaY * PanScale);
        if (!FEditorSettings::Get().bEnableCameraSmoothing)
        {
            Camera->SetLocation(TargetLocation);
        }
    }
    else
    {
        // Accumulate yaw/pitch and rebuild rotation quaternion
        const float RotationSpeed = FEditorSettings::Get().CameraRotationSpeed / 400.0f;
        Yaw   += DeltaX * RotationSpeed;
        Pitch -= DeltaY * RotationSpeed;
        Pitch  = MathUtil::Clamp(Pitch, -89.f, 89.f);
        UpdateCameraRotation();
    }
}

void FEditorWorldController::OnKeyPressed(int VK)
{
    switch (VK)
    {
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
        if (Camera)
        {
            SeedYawPitchFromCamera();
        }
        break;
    }
}

void FEditorWorldController::OnKeyDown(int VK)
{
    if (!Camera)
        return;

    // 수식 키(Ctrl, Alt, Shift)가 눌려 있으면 뷰포트 이동을 차단합니다.
    const InputSystem& IS = InputSystem::Get();
    if (IS.GetKey(VK_CONTROL) || IS.GetKey(VK_MENU) || IS.GetKey(VK_SHIFT))
        return;

    if (Camera->IsOrthographic())
        return; // no WASD/arrow input in ortho views

    const FEditorSettings& Settings = FEditorSettings::Get();
    const float EffectiveMoveSpeed = MoveSpeed;

    // WASD + QE movement — scale by current camera forward/right vectors
    FVector Move = FVector(0, 0, 0);
    switch (VK)
    {
    case 'W': Move += Camera->GetForwardVector() *  EffectiveMoveSpeed * DeltaTime; break;
    case 'S': Move += Camera->GetForwardVector() * -EffectiveMoveSpeed * DeltaTime; break;
    case 'D': Move += Camera->GetRightVector()   *  EffectiveMoveSpeed * DeltaTime; break;
    case 'A': Move += Camera->GetRightVector()   * -EffectiveMoveSpeed * DeltaTime; break;
    case 'E': Move += FVector(0, 0, 1)           *  EffectiveMoveSpeed * DeltaTime; break;
    case 'Q': Move += FVector(0, 0, 1)           * -EffectiveMoveSpeed * DeltaTime; break;
    }
    TargetLocation += Move;
    if (!FEditorSettings::Get().bEnableCameraSmoothing)
    {
        Camera->SetLocation(TargetLocation);
    }

    // Arrow key rotation
    const float AngleVelocity = Settings.CameraRotationSpeed;
    bool bRotationChanged = false;
    switch (VK)
    {
    case VK_LEFT:  Yaw   -= AngleVelocity * DeltaTime; bRotationChanged = true; break;
    case VK_RIGHT: Yaw   += AngleVelocity * DeltaTime; bRotationChanged = true; break;
    case VK_UP:    Pitch += AngleVelocity * DeltaTime; bRotationChanged = true; break;
    case VK_DOWN:  Pitch -= AngleVelocity * DeltaTime; bRotationChanged = true; break;
    }
    if (bRotationChanged)
    {
        Pitch = MathUtil::Clamp(Pitch, -89.f, 89.f);
        UpdateCameraRotation();
    }
}

void FEditorWorldController::OnKeyReleased(int VK)
{
    // Nothing for now
}

void FEditorWorldController::OnWheelScrolled(float Notch)
{
    if (!Camera || Notch == 0.f)
        return;

    if (Camera->IsOrthographic())
    {
        float NewWidth = Camera->GetOrthoHeight() - Notch * FEditorSettings::Get().CameraZoomSpeed * DeltaTime;
        Camera->SetOrthoHeight(MathUtil::Clamp(NewWidth, 0.1f, 1000.0f));
    }
    else
    {
        const FVector Forward = Camera->GetForwardVector().GetSafeNormal();
        const float DollyDistance = Notch * std::max(0.1f, MoveSpeed) * FEditorSettings::Get().CameraDollySpeedScale * 0.35f;
        TargetLocation += Forward * DollyDistance;
        if (!FEditorSettings::Get().bEnableCameraSmoothing)
        {
            Camera->SetLocation(TargetLocation);
        }
    }
}

void FEditorWorldController::OnMiddleMouseDrag(float DeltaX, float DeltaY)
{
    if (!Camera)
        return;

    // 수식 키(Ctrl, Alt, Shift)가 눌려 있으면 뷰포트 이동을 차단합니다.
    const InputSystem& IS = InputSystem::Get();
    if (IS.GetKey(VK_CONTROL) || IS.GetKey(VK_MENU) || IS.GetKey(VK_SHIFT))
        return;

    const float   PanScale = (Camera->IsOrthographic()
                                 ? Camera->GetOrthoHeight() * 0.002f
                                 : std::max(1.0f, MoveSpeed) * 0.002f)
                             * FEditorSettings::Get().CameraPanSpeedScale;
    const FVector Right    = Camera->GetEffectiveRight();
    const FVector Up       = Camera->GetEffectiveUp();
    TargetLocation += Right * (DeltaX * PanScale) + Up * (-DeltaY * PanScale);
    if (!FEditorSettings::Get().bEnableCameraSmoothing)
    {
        Camera->SetLocation(TargetLocation);
    }
}

void FEditorWorldController::SetSelectionManager(FSelectionManager* InSM)
{
    if (InSM)
        SelectionManager = InSM;
    if (SelectionManager->GetGizmo())
        Gizmo = SelectionManager->GetGizmo();
}

void FEditorWorldController::SetSelectionManager(FSelectionManager& InSM)
{
    SelectionManager = &InSM;
    if (SelectionManager->GetGizmo())
        Gizmo = SelectionManager->GetGizmo();
}

void FEditorWorldController::SetGizmo(UGizmoComponent* InGizmo)
{
    if (InGizmo)
        Gizmo = InGizmo;
}

void FEditorWorldController::SetGizmo(UGizmoComponent& InGizmo) { Gizmo = &InGizmo; }

void FEditorWorldController::UpdateCameraRotation()
{
    if (!Camera)
        return;

    const float PitchRad = MathUtil::DegreesToRadians(Pitch);
    const float YawRad   = MathUtil::DegreesToRadians(Yaw);

    FVector Forward(
        std::cos(PitchRad) * std::cos(YawRad),
        std::cos(PitchRad) * std::sin(YawRad),
        std::sin(PitchRad));
    Forward = Forward.GetSafeNormal();

    FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
    if (Right.IsNearlyZero())
        return;

    FVector Up = FVector::CrossProduct(Forward, Right).GetSafeNormal();

    FMatrix RotMat = FMatrix::Identity;
    RotMat.SetAxes(Forward, Right, Up);

    FQuat NewRotation(RotMat);
    NewRotation.Normalize();
    TargetRotation = NewRotation;
    bTargetRotationInitialized = true;
    if (!FEditorSettings::Get().bEnableCameraSmoothing)
    {
        Camera->SetRotation(NewRotation);
    }
}

void FEditorWorldController::SeedYawPitchFromCamera()
{
    if (!Camera)
    {
        return;
    }

    const FVector Forward = Camera->GetForwardVector().GetSafeNormal();
    Pitch = MathUtil::RadiansToDegrees(std::asin(MathUtil::Clamp(Forward.Z, -1.f, 1.f)));
    Yaw = MathUtil::RadiansToDegrees(std::atan2(Forward.Y, Forward.X));
}

void FEditorWorldController::ClearPendingSelectionPress()
{
    bHasPendingSelectionPress = false;
    PendingSelectionPressX = 0.0f;
    PendingSelectionPressY = 0.0f;
}
