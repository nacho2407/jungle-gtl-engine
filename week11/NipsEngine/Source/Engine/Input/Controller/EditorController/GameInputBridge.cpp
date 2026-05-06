#include "GameInputBridge.h"
#include "Camera/ViewportCamera.h"
#include "Engine/Input/InputSystem.h"
#include "GameFramework/PlayerController.h"

#include <windows.h>

void FGameInputBridge::Tick(float InDeltaTime) {
	DeltaTime = InDeltaTime;
    if (PlayerController)
    {
        if (const FViewportCamera* RuntimeCamera = PlayerController->GetRuntimeCamera())
        {
            TargetLocation = RuntimeCamera->GetLocation();
            bTargetLocationInitialized = true;
        }
        return;
    }
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

void FGameInputBridge::OnMouseMove(float DeltaX, float DeltaY)
{
    if (PlayerController)
    {
        PlayerController->HandleMouseMove(DeltaX, DeltaY);
        return;
    }

	if (!Camera)
        return;

    if (Camera->IsOrthographic())
    {
        // Pan: scale movement proportionally to current ortho zoom level
        const float   PanScale = Camera->GetOrthoHeight() * 0.002f;
        const FVector Right = Camera->GetEffectiveRight();
        const FVector Up = Camera->GetEffectiveUp();
        TargetLocation += Right * (-DeltaX * PanScale) + Up * (DeltaY * PanScale);
    }
    else
    {
        // Accumulate yaw/pitch and rebuild rotation quaternion
        const float RotationSpeed = 0.15f;
        Yaw += DeltaX * RotationSpeed;
        Pitch -= DeltaY * RotationSpeed;
        Pitch = MathUtil::Clamp(Pitch, -89.f, 89.f);
        UpdateCameraRotation();
    }
}

void FGameInputBridge::OnMouseMoveAbsolute(float X, float Y)
{
    if (PlayerController)
    {
        PlayerController->HandleMouseMoveAbsolute(X, Y);
    }
}

void FGameInputBridge::OnLeftMouseClick(float X, float Y)
{
    if (PlayerController)
    {
        PlayerController->HandleMouseButtonPressed(VK_LBUTTON, X, Y);
        return;
    }
}

void FGameInputBridge::OnLeftMouseDragEnd(float X, float Y)
{
    if (PlayerController)
    {
        PlayerController->HandleMouseDragEnd(VK_LBUTTON, X, Y);
        PlayerController->HandleMouseButtonReleased(VK_LBUTTON, X, Y);
    }
}

void FGameInputBridge::OnLeftMouseButtonUp(float X, float Y)
{
    if (PlayerController)
    {
        PlayerController->HandleMouseButtonReleased(VK_LBUTTON, X, Y);
    }
}

void FGameInputBridge::OnRightMouseClick(float DeltaX, float DeltaY)
{
    if (PlayerController)
    {
        PlayerController->HandleMouseButtonPressed(VK_RBUTTON, DeltaX, DeltaY);
    }
}

void FGameInputBridge::OnLeftMouseDrag(float X, float Y)
{
    if (PlayerController)
    {
        PlayerController->HandleMouseDrag(VK_LBUTTON, X, Y);
    }
}

void FGameInputBridge::OnRightMouseDrag(float DeltaX, float DeltaY)
{
    if (PlayerController)
    {
        PlayerController->HandleMouseDrag(VK_RBUTTON, DeltaX, DeltaY);
        PlayerController->HandleMouseButtonDown(VK_RBUTTON, DeltaX, DeltaY);
    }
}

void FGameInputBridge::OnMiddleMouseDrag(float DeltaX, float DeltaY)
{
    if (PlayerController)
    {
        PlayerController->HandleMouseDrag(VK_MBUTTON, DeltaX, DeltaY);
        PlayerController->HandleMouseButtonDown(VK_MBUTTON, DeltaX, DeltaY);
    }
}

void FGameInputBridge::OnKeyPressed(int VK)
{
    if (PlayerController)
    {
        PlayerController->HandleKeyPressed(VK);
    }
}

void FGameInputBridge::OnKeyDown(int VK)
{
    if (PlayerController)
    {
        PlayerController->HandleKeyDown(VK);
        return;
    }

    // WASD continuous movement + arrow key rotation (called every frame the key is held)
    if (!Camera)
        return;

    const float MoveSpeed = 10.f;
    FVector     Move = FVector(0, 0, 0);
    //switch (VK)
    //{
    //case 'W':
    //    Move.X += MoveSpeed;
    //    break;
    //case 'A':
    //    Move.Y -= MoveSpeed;
    //    break;
    //case 'S':
    //    Move.X -= MoveSpeed;
    //    break;
    //case 'D':
    //    Move.Y += MoveSpeed;
    //    break;
    //}

    //if (Move.X != 0.f || Move.Y != 0.f)
    //{
    //    // Constrain movement to the horizontal plane (no flying)
    //    Move *= DeltaTime;
    //    FVector Forward = Camera->GetForwardVector();
    //    Forward.Z = 0.f;
    //    if (Forward.Size() > 1e-4f)
    //        Forward.Normalize();
    //    FVector Right = Camera->GetRightVector();
    //    Right.Z = 0.f;
    //    if (Right.Size() > 1e-4f)
    //        Right.Normalize();
    //    TargetLocation += Forward * Move.X + Right * Move.Y;
    //}

	// Allow flying
	switch (VK)
    {
    case 'W':
        Move += Camera->GetForwardVector() * MoveSpeed;
        break;
    case 'S':
        Move += Camera->GetForwardVector() * -MoveSpeed;
        break;
    case 'D':
        Move += Camera->GetRightVector() * MoveSpeed;
        break;
    case 'A':
        Move += Camera->GetRightVector() * -MoveSpeed;
        break;
	}

    if (Move.X != 0.f || Move.Y != 0.f)
    {
        // Constrain movement to the horizontal plane (no flying)
        Move *= DeltaTime;
        TargetLocation += Move;
    }

    // Arrow key rotation
    constexpr float AngleVelocity = 60.f;
    bool            bRotationChanged = false;
    switch (VK)
    {
    case VK_LEFT:
        Yaw -= AngleVelocity * DeltaTime;
        bRotationChanged = true;
        break;
    case VK_RIGHT:
        Yaw += AngleVelocity * DeltaTime;
        bRotationChanged = true;
        break;
    case VK_UP:
        Pitch += AngleVelocity * DeltaTime;
        bRotationChanged = true;
        break;
    case VK_DOWN:
        Pitch -= AngleVelocity * DeltaTime;
        bRotationChanged = true;
        break;
    }
    if (bRotationChanged)
    {
        Pitch = MathUtil::Clamp(Pitch, -89.f, 89.f);
        UpdateCameraRotation();
    }
}


void FGameInputBridge::OnKeyReleased(int VK)
{
    if (PlayerController)
    {
        PlayerController->HandleKeyReleased(VK);
    }
}

void FGameInputBridge::OnWheelScrolled(float Notch)
{
    if (PlayerController)
    {
        PlayerController->HandleMouseWheel(Notch);
    }
}

void FGameInputBridge::SetCamera(FViewportCamera* InCamera)
{
    if (!InCamera)
        return;
    Camera = InCamera;

	TargetLocation = Camera->GetLocation();
    bTargetLocationInitialized = true;

    // Initialize Yaw/Pitch from camera's current orientation so the first
    // mouse move does not snap the camera to a default forward direction.
    FVector Forward = Camera->GetForwardVector();
    Pitch = MathUtil::RadiansToDegrees(std::asin(MathUtil::Clamp(Forward.Z, -1.f, 1.f)));
    Yaw   = MathUtil::RadiansToDegrees(std::atan2(Forward.Y, Forward.X));
}

void FGameInputBridge::SetCamera(FViewportCamera& InCamera) { SetCamera(&InCamera); }

void FGameInputBridge::UpdateCameraRotation()
{
    if (!Camera)
        return;

    const float PitchRad = MathUtil::DegreesToRadians(Pitch);
    const float YawRad = MathUtil::DegreesToRadians(Yaw);

    FVector Forward(std::cos(PitchRad) * std::cos(YawRad), std::cos(PitchRad) * std::sin(YawRad), std::sin(PitchRad));
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

void FGameInputBridge::ResetTargetLocation()
{
    if (Camera)
        TargetLocation = Camera->GetLocation();
}
