#include "EditorGizmoTool.h"

#include "Collision/RayUtils.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "GameFramework/World.h"
#include "Viewport/Viewport.h"

bool FEditorGizmoTool::HandleInput(float DeltaTime)
{
    (void)DeltaTime;

    if (!Owner || !Owner->GetCamera() || !Owner->GetGizmo() || !Owner->GetWorld())
    {
        return false;
    }

    UCameraComponent* Camera = Owner->GetCamera();
    UGizmoComponent* Gizmo = Owner->GetGizmo();

	if (Owner->GetWorld()->GetWorldType() == EWorldType::PIE)
    {
        if (Gizmo->IsHolding())
        {
            Gizmo->DragEnd();
        }
        else if (Gizmo->IsPressedOnHandle())
        {
            Gizmo->SetPressedOnHandle(false);
        }

        return false;
    }

    Gizmo->ApplyScreenSpaceScaling(Camera->GetWorldLocation(), Camera->IsOrthogonal(), Camera->GetOrthoWidth());

    Gizmo->SetAxisMask(UGizmoComponent::ComputeAxisMask(Owner->GetRenderOptions().ViewportType, Gizmo->GetMode()));

    FRay Ray;
    if (!BuildMouseRay(Ray))
    {
        return Gizmo->IsHolding() || Gizmo->IsPressedOnHandle();
    }

    const FEditorViewportFrameInput& Input = Owner->GetCurrentInput();
    const bool bMouseMoved = Input.MouseAxisDelta.x != 0 || Input.MouseAxisDelta.y != 0;
    const bool bLeftDragging = Input.bLeftDown && bMouseMoved;

    if (Input.bLeftReleased)
    {
        if (Gizmo->IsHolding() && bMouseMoved)
        {
            Gizmo->UpdateDrag(Ray);
        }

        return HandleDragEnd();
    }

    if (Input.bLeftPressed)
    {
        return HandleDragStart(Ray);
    }

    if (bLeftDragging)
    {
        return HandleDragUpdate(Ray);
    }

    return Gizmo->IsHolding() || Gizmo->IsPressedOnHandle();
}

bool FEditorGizmoTool::BuildMouseRay(FRay& OutRay) const
{
    if (!Owner || !Owner->GetCamera())
    {
        return false;
    }

    FViewport* Viewport = Owner->GetViewport();
    const FRect& ViewportRect = Owner->GetViewportScreenRect();

    const float VPWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : ViewportRect.Width;
    const float VPHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : ViewportRect.Height;
    if (VPWidth <= 0.0f || VPHeight <= 0.0f)
    {
        return false;
    }

    const FEditorViewportFrameInput& Input = Owner->GetCurrentInput();
    const float LocalMouseX = static_cast<float>(Input.MouseLocalPos.x);
    const float LocalMouseY = static_cast<float>(Input.MouseLocalPos.y);

    OutRay = Owner->GetCamera()->DeprojectScreenToWorld(LocalMouseX, LocalMouseY, VPWidth, VPHeight);
    return true;
}

bool FEditorGizmoTool::HandleDragStart(const FRay& Ray)
{
    UGizmoComponent* Gizmo = Owner ? Owner->GetGizmo() : nullptr;
    if (!Gizmo)
    {
        return false;
    }

    FHitResult HitResult{};

    if (FRayUtils::RaycastComponent(Gizmo, Ray, HitResult))
    {
        Gizmo->SetPressedOnHandle(true);
        return true;
    }

	Gizmo->SetPressedOnHandle(false);
    return false;
}

bool FEditorGizmoTool::HandleDragUpdate(const FRay& Ray)
{
    UGizmoComponent* Gizmo = Owner ? Owner->GetGizmo() : nullptr;
    if (!Gizmo)
    {
        return false;
    }

    if (Gizmo->IsPressedOnHandle() && !Gizmo->IsHolding())
    {
        Gizmo->SetHolding(true);
    }

    if (Gizmo->IsHolding())
    {
        Gizmo->UpdateDrag(Ray);
        return true;
    }

    return false;
}

bool FEditorGizmoTool::HandleDragEnd()
{
    UGizmoComponent* Gizmo = Owner ? Owner->GetGizmo() : nullptr;
    if (!Gizmo)
    {
        return false;
    }

    if (Gizmo->IsHolding())
    {
        Gizmo->DragEnd();
        return true;
    }

    if (Gizmo->IsPressedOnHandle())
    {
        Gizmo->SetPressedOnHandle(false);
        return true;
    }

    return false;
}
