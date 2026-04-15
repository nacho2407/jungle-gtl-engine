#include "EditorWorldController.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/Viewport/ViewportCamera.h"
#include "Engine/Component/GizmoComponent.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Level.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Collision/RayCollision/RayCollision.h"
#include "Math/Utils.h"

#include <cmath>
#include <windows.h>

void FEditorWorldController::SetCamera(FViewportCamera* InCamera)
{
    if (!InCamera)
        return;
    Camera = InCamera;
    TargetLocation = InCamera->GetLocation();
    bTargetLocationInitialized = true;
}

void FEditorWorldController::SetCamera(FViewportCamera& InCamera)
{
    Camera = &InCamera;
    TargetLocation = InCamera.GetLocation();
    bTargetLocationInitialized = true;
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
    constexpr float LocationLerpSpeed = 12.0f;
    const FVector   CurrentLocation = Camera->GetLocation();
    const float     LerpAlpha = MathUtil::Clamp(DeltaTime * LocationLerpSpeed, 0.0f, 1.0f);
    Camera->SetLocation(CurrentLocation + (TargetLocation - CurrentLocation) * LerpAlpha);
}

// X/Y parameters for mouse events are viewport-local pixel coordinates.
// DeltaX/DeltaY parameters are raw mouse movement deltas (pixels since last frame).
void FEditorWorldController::OnMouseMoveAbsolute(float X, float Y)
{
    if (!Gizmo || !Camera)
        return;

    // Update gizmo hover highlight each frame
    FRay       Ray = Camera->DeprojectScreenToWorld(X, Y, ViewportWidth, ViewportHeight);
    FHitResult HitResult;
    Gizmo->RaycastMesh(Ray, HitResult);
}

void FEditorWorldController::OnLeftMouseClick(float X, float Y)
{
    // Try gizmo handle first, then actor selection
    if (!Camera)
        return;

    FRay       Ray = Camera->DeprojectScreenToWorld(X, Y, ViewportWidth, ViewportHeight);
    FHitResult HitResult{};

    if (Gizmo && FRayCollision::RaycastComponent(Gizmo, Ray, HitResult))
    {
        Gizmo->SetPressedOnHandle(true);
        return;
    }
    else
    {
        Gizmo->SetPressedOnHandle(false);
	}

    // Actor selection
    if (!World || !SelectionManager)
        return;

    AActor* BestActor = nullptr;
    float   ClosestDist = FLT_MAX;

	FWorldSpatialIndex::FPrimitiveRayQueryScratch RayQueryScratch;
	TArray<UPrimitiveComponent*> CandidatePrimitives;
    TArray<float>                CandidateTs;
    World->GetSpatialIndex().RayQueryPrimitives(Ray, CandidatePrimitives, CandidateTs, RayQueryScratch);

	InputSystem& IS = InputSystem::Get();

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

    bool bCtrl = IS.GetKey(VK_CONTROL);
    if (!BestActor)
    {
        if (!bCtrl)
            SelectionManager->ClearSelection();
    }
    else
    {
        if (bCtrl)
            SelectionManager->ToggleSelect(BestActor);
        else
            SelectionManager->Select(BestActor);
    }
}

void FEditorWorldController::OnLeftMouseDrag(float X, float Y)
{
    if (!Gizmo || !Camera)
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
    if (Gizmo)
        Gizmo->DragEnd();
}

void FEditorWorldController::OnLeftMouseButtonUp(float X, float Y)
{
    (void)X;
    (void)Y;
    // LMB released without reaching drag threshold — disarm gizmo
    if (Gizmo)
        Gizmo->SetPressedOnHandle(false);
}

void FEditorWorldController::OnRightMouseClick(float DeltaX, float DeltaY)
{
    // Seed Yaw/Pitch from current camera orientation so the first drag doesn't snap
    if (!Camera)
        return;
    const FVector Forward = Camera->GetForwardVector().GetSafeNormal();
    Pitch = MathUtil::RadiansToDegrees(std::asin(MathUtil::Clamp(Forward.Z, -1.f, 1.f)));
    Yaw   = MathUtil::RadiansToDegrees(std::atan2(Forward.Y, Forward.X));
}

void FEditorWorldController::OnRightMouseDrag(float DeltaX, float DeltaY)
{
    if (!Camera)
        return;

    if (Camera->IsOrthographic())
    {
        // Pan: scale movement proportionally to current ortho zoom level
        const float     PanScale = Camera->GetOrthoHeight() * 0.002f;
        const FVector   Right    = Camera->GetEffectiveRight();
        const FVector   Up       = Camera->GetEffectiveUp();
        TargetLocation += Right * (-DeltaX * PanScale) + Up * (DeltaY * PanScale);
    }
    else
    {
        // Accumulate yaw/pitch and rebuild rotation quaternion
        constexpr float RotationSpeed = 0.15f;
        Yaw   += DeltaX * RotationSpeed;
        Pitch -= DeltaY * RotationSpeed;
        Pitch  = MathUtil::Clamp(Pitch, -89.f, 89.f);
        UpdateCameraRotation();
    }
}

void FEditorWorldController::OnKeyPressed(int VK)
{
	InputSystem& IS = InputSystem::Get();

    switch (VK)
    {
    case 'D':
        if (IS.GetKey(VK_CONTROL))
        {
            if (World && SelectionManager && !SelectionManager->IsEmpty())
            {
                TArray<AActor*> DuplicatedActors;
                const TArray<AActor*>& SelectedActors = SelectionManager->GetSelectedActors();
                for (AActor* OriginalActor : SelectedActors)
                {
                    if (!OriginalActor) continue;

                    AActor* NewActor = Cast<AActor>(OriginalActor->Duplicate());
                    if (NewActor)
                    {
                        NewActor->SetWorld(World);
                        if (World->HasBegunPlay())
                        {
                            NewActor->BeginPlay();
                        }
                        World->GetPersistentLevel()->AddActor(NewActor);
                        DuplicatedActors.push_back(NewActor);
                    }
                }

                if (!DuplicatedActors.empty())
                {
                    SelectionManager->ClearSelection();
                    for (AActor* NewActor : DuplicatedActors)
                    {
                        SelectionManager->AddSelect(NewActor);
                    }
                }
            }
        }
        break;
    case VK_SPACE:
        if (Gizmo)
            Gizmo->SetNextMode();
        break;
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
        if (Camera)
        {
            const FVector Forward = Camera->GetForwardVector().GetSafeNormal();
            Pitch = MathUtil::RadiansToDegrees(std::asin(MathUtil::Clamp(Forward.Z, -1.f, 1.f)));
            Yaw   = MathUtil::RadiansToDegrees(std::atan2(Forward.Y, Forward.X));
        }
        break;
    }
}

void FEditorWorldController::OnKeyDown(int VK)
{
    if (!Camera)
        return;
    if (Camera->IsOrthographic())
        return; // no WASD/arrow input in ortho views

    // WASD + QE movement — scale by current camera forward/right vectors
    FVector Move = FVector(0, 0, 0);
    switch (VK)
    {
    case 'W': Move += Camera->GetForwardVector() *  MoveSpeed * DeltaTime; break;
    case 'S': Move += Camera->GetForwardVector() * -MoveSpeed * DeltaTime; break;
    case 'D': Move += Camera->GetRightVector()   *  MoveSpeed * DeltaTime; break;
    case 'A': Move += Camera->GetRightVector()   * -MoveSpeed * DeltaTime; break;
    case 'E': Move += FVector(0, 0, 1)           *  MoveSpeed * DeltaTime; break;
    case 'Q': Move += FVector(0, 0, 1)           * -MoveSpeed * DeltaTime; break;
    }
    TargetLocation += Move;

    // Arrow key rotation
    constexpr float AngleVelocity = 60.f;
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
        float NewWidth = Camera->GetOrthoHeight() - Notch * 300.f * DeltaTime;
        Camera->SetOrthoHeight(MathUtil::Clamp(NewWidth, 0.1f, 1000.0f));
    }
    else
    {
        constexpr float FovStep = 2.0f * MathUtil::DEG_TO_RAD;
        float           NewFOV = Camera->GetFOV() - Notch * FovStep;
        Camera->SetFOV(MathUtil::Clamp(NewFOV, 1.f * MathUtil::DEG_TO_RAD, 90.0f * MathUtil::DEG_TO_RAD));
    }
}

void FEditorWorldController::OnMiddleMouseDrag(float DeltaX, float DeltaY)
{
    if (!Camera)
        return;
    const float   PanScale = Camera->IsOrthographic()
                                 ? Camera->GetOrthoHeight() * 0.002f
                                 : 20.0f;
    const FVector Right    = Camera->GetEffectiveRight();
    const FVector Up       = Camera->GetEffectiveUp();
    TargetLocation += Right * (-DeltaX * PanScale * DeltaTime) + Up * (DeltaY * PanScale * DeltaTime);
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
    Camera->SetRotation(NewRotation);
}
