#include "EditorNavigationTool.h"

#include "Editor/Settings/EditorSettings.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Engine/Component/CameraComponent.h"
#include "Engine/Math/MathUtils.h"

bool FEditorNavigationTool::HandleInput(float DeltaTime)
{
    if (!Owner || !Owner->GetCamera())
    {
        return false;
    }

    bool bHandled = false;
    bHandled |= HandleKeyboardAndMouseNavigation(DeltaTime);
    bHandled |= HandleWheelZoom(DeltaTime);
    return bHandled;
}

bool FEditorNavigationTool::HandleKeyboardAndMouseNavigation(float DeltaTime)
{
    UCameraComponent* Camera = Owner->GetCamera();
    if (!Camera)
    {
        return false;
    }

    const FEditorViewportFrameInput& Input = Owner->GetCurrentInput();
    const FViewportRenderOptions& RenderOptions = Owner->GetRenderOptions();
    const FEditorSettings* Settings = Owner->GetSettings();
    const FCameraState& CameraState = Camera->GetCameraState();
    const bool bIsOrtho = CameraState.bIsOrthogonal;

    const float MoveSensitivity = RenderOptions.CameraMoveSensitivity;
    const float CameraSpeed = (Settings ? Settings->CameraSpeed : 10.f) * MoveSensitivity;
    const float PanMouseScale = CameraSpeed * 0.01f;

    bool bHandled = false;

    if (!bIsOrtho)
    {
        FVector LocalMove = FVector(0, 0, 0);
        float WorldVerticalMove = 0.0f;

        if (Input.KeyDown['W'])
            LocalMove.X += CameraSpeed;
        if (Input.KeyDown['A'])
            LocalMove.Y -= CameraSpeed;
        if (Input.KeyDown['S'])
            LocalMove.X -= CameraSpeed;
        if (Input.KeyDown['D'])
            LocalMove.Y += CameraSpeed;
        if (Input.KeyDown['Q'])
            WorldVerticalMove -= CameraSpeed;
        if (Input.KeyDown['E'])
            WorldVerticalMove += CameraSpeed;

        if (LocalMove.X != 0.0f || LocalMove.Y != 0.0f || LocalMove.Z != 0.0f)
        {
            LocalMove *= DeltaTime;
            Camera->MoveLocal(LocalMove);
            bHandled = true;
        }

        if (WorldVerticalMove != 0.0f)
        {
            Camera->AddWorldOffset(FVector(0.0f, 0.0f, WorldVerticalMove * DeltaTime));
            bHandled = true;
        }

        if (Input.bMiddleDown)
        {
            const float DeltaX = static_cast<float>(Input.MouseAxisDelta.x);
            const float DeltaY = static_cast<float>(Input.MouseAxisDelta.y);
            if (DeltaX != 0.0f || DeltaY != 0.0f)
            {
                Camera->MoveLocal(FVector(0.0f, -DeltaX * PanMouseScale * 0.05f, DeltaY * PanMouseScale * 0.05f));
                bHandled = true;
            }
        }

        FVector Rotation = FVector(0, 0, 0);

        const float RotateSensitivity = RenderOptions.CameraRotateSensitivity;
        const float AngleVelocity = (Settings ? Settings->CameraRotationSpeed : 60.f) * RotateSensitivity;
        if (Input.KeyDown[VK_UP])
            Rotation.Z -= AngleVelocity;
        if (Input.KeyDown[VK_LEFT])
            Rotation.Y -= AngleVelocity;
        if (Input.KeyDown[VK_DOWN])
            Rotation.Z += AngleVelocity;
        if (Input.KeyDown[VK_RIGHT])
            Rotation.Y += AngleVelocity;

        FVector MouseRotation = FVector(0, 0, 0);
        const float MouseRotationSpeed = 0.15f * RotateSensitivity;

        if (Input.bRightDown)
        {
            const float DeltaX = static_cast<float>(Input.MouseAxisDelta.x);
            const float DeltaY = static_cast<float>(Input.MouseAxisDelta.y);

            MouseRotation.Y += DeltaX * MouseRotationSpeed;
            MouseRotation.Z += DeltaY * MouseRotationSpeed;
        }

        Rotation *= DeltaTime;
        if (Rotation.Y != 0.0f || Rotation.Z != 0.0f || MouseRotation.Y != 0.0f || MouseRotation.Z != 0.0f)
        {
            Camera->Rotate(Rotation.Y + MouseRotation.Y, Rotation.Z + MouseRotation.Z);
            bHandled = true;
        }
    }
    else
    {
        if (Input.bRightDown)
        {
            const float DeltaX = static_cast<float>(Input.MouseAxisDelta.x);
            const float DeltaY = static_cast<float>(Input.MouseAxisDelta.y);
            if (DeltaX != 0.0f || DeltaY != 0.0f)
            {
                const float PanScale = CameraState.OrthoWidth * 0.002f * MoveSensitivity;
                Camera->MoveLocal(FVector(0, -DeltaX * PanScale, DeltaY * PanScale));
                bHandled = true;
            }
        }
    }

    return bHandled;
}

bool FEditorNavigationTool::HandleWheelZoom(float DeltaTime)
{
    UCameraComponent* Camera = Owner ? Owner->GetCamera() : nullptr;
    if (!Camera)
    {
        return false;
    }

    const FEditorViewportFrameInput& Input = Owner->GetCurrentInput();
    const float ScrollNotches = Input.MouseWheelNotches;
    if (ScrollNotches == 0.0f)
    {
        return false;
    }

    const FEditorSettings* Settings = Owner->GetSettings();
    const float ZoomSpeed = Settings ? Settings->CameraZoomSpeed : 300.f;

    if (Camera->IsOrthogonal())
    {
        const float NewWidth = Camera->GetOrthoWidth() - ScrollNotches * ZoomSpeed * 0.015f;
        Camera->SetOrthoWidth(Clamp(NewWidth, 0.1f, 1000.0f));
    }
    else
    {
        Camera->MoveLocal(FVector(ScrollNotches * ZoomSpeed * 0.015f, 0.0f, 0.0f));
    }

    return true;
}
