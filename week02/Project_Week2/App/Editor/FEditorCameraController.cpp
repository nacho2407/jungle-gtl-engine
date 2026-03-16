#define NOMINMAX
#include <algorithm>

#include "Editor/FEditorCameraController.h"

#include "WindowsApplication.h"
#include "Component/UCameraComponent.h"
#include "Math/FVector.h"
#include "Math/FVector4.h"
#include "Math/FMatrix.h"

void FEditorCameraController::Initialize(FWindowsApplication* InWindowApp, UCameraComponent* InCamera)
{
    WindowApp = InWindowApp;
    Camera = InCamera;
}

void FEditorCameraController::Tick(float DeltaTime)
{
    if (!WindowApp || !Camera)
    {
        return;
    }

    int RightDownX = 0;
    int RightDownY = 0;
    if (WindowApp->ConsumeRightMouseDown(RightDownX, RightDownY))
    {
        PrevMouseX = RightDownX;
        PrevMouseY = RightDownY;
    }

    int MouseX = 0;
    int MouseY = 0;

    if (WindowApp->IsRightMousePressed())
    {
        WindowApp->GetMousePosition(MouseX, MouseY);

        int DeltaX = MouseX - PrevMouseX;
        int DeltaY = MouseY - PrevMouseY;

        FVector Rot = Camera->GetRelativeRotation();

        const float RotateSpeed = 0.005f;

        Rot.Y += DeltaX * RotateSpeed;
        Rot.X += DeltaY * RotateSpeed;

        Rot.X = std::clamp(Rot.X, -1.5f, 1.5f);

        Camera->SetRelativeRotation(Rot);

        PrevMouseX = MouseX;
        PrevMouseY = MouseY;
    }
    else
    {
        WindowApp->GetMousePosition(PrevMouseX, PrevMouseY);
    }

    FVector CameraLoc = Camera->GetRelativeLocation();
    FVector Rot = Camera->GetRelativeRotation();

    FMatrix RotMatrix = FMatrix::MakeRotationXYZ(Rot);

    FVector4 Forward4 = RotMatrix * FVector4(0, 0, 1, 0);
    FVector4 Right4 = RotMatrix * FVector4(1, 0, 0, 0);
    FVector4 Up4 = RotMatrix * FVector4(0, 1, 0, 0);

    FVector Forward(Forward4.X, Forward4.Y, Forward4.Z);
    FVector Right(Right4.X, Right4.Y, Right4.Z);
    FVector Up(Up4.X, Up4.Y, Up4.Z);

    float Speed = 5.0f * DeltaTime;

    if (WindowApp->IsKeyDown('W'))
        CameraLoc += Forward * Speed;

    if (WindowApp->IsKeyDown('S'))
        CameraLoc -= Forward * Speed;

    if (WindowApp->IsKeyDown('A'))
        CameraLoc -= Right * Speed;

    if (WindowApp->IsKeyDown('D'))
        CameraLoc += Right * Speed;

    if (WindowApp->IsKeyDown('Q'))
        CameraLoc -= Up * Speed;

    if (WindowApp->IsKeyDown('E'))
        CameraLoc += Up * Speed;

    Camera->SetRelativeLocation(CameraLoc);

    float Wheel = WindowApp->ConsumeMouseWheelDelta();
    if (Wheel != 0.0f)
    {
        FVector Loc = Camera->GetRelativeLocation();
        FVector CamRot = Camera->GetRelativeRotation();

        FMatrix CamRotMatrix = FMatrix::MakeRotationXYZ(CamRot);

        FVector4 CamForward4 = CamRotMatrix * FVector4(0, 0, 1, 0);
        FVector CamForward(CamForward4.X, CamForward4.Y, CamForward4.Z);

        float DollySpeed = 2.0f;

        Loc += CamForward * Wheel * DollySpeed;
        Camera->SetRelativeLocation(Loc);
    }
}