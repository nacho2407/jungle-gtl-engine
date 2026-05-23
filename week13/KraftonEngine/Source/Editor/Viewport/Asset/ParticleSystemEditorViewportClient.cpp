#include "ParticleSystemEditorViewportClient.h"

#include "Component/Primitive/ParticleSystemComponent.h"
#include "Input/InputSystem.h"
#include "Math/MathUtils.h"
#include "Render/Types/MinimalViewInfo.h"
#include "Settings/EditorSettings.h"
#include "Slate/SlateApplication.h"
#include "Viewport/Viewport.h"

#include <cmath>
#include <imgui.h>

void FParticleSystemEditorViewportClient::Initialize(ID3D11Device* Device, uint32 Width, uint32 Height)
{
    Viewport = new FViewport();
    Viewport->Initialize(Device, Width, Height);
    Viewport->SetClient(this);

    bIsRenderable = true;
}

void FParticleSystemEditorViewportClient::Release()
{
    if (Viewport)
    {
        Viewport->Release();
        delete Viewport;
        Viewport = nullptr;
    }

    PreviewWorld                   = nullptr;
    PreviewActor                   = nullptr;
    PreviewParticleSystemComponent = nullptr;
    bIsRenderable                  = false;
}

void FParticleSystemEditorViewportClient::ResetCameraToPreviewBounds()
{
    FBoundingBox Bounds = PreviewParticleSystemComponent ? PreviewParticleSystemComponent->GetWorldBoundingBox()
    : FBoundingBox(FVector(-100.0f, -100.0f, -100.0f), FVector(100.0f, 100.0f, 100.0f));

    FVector Center = Bounds.GetCenter();
    float   Radius = Bounds.GetExtent().Length();
    if (Radius < 0.1f)
    {
        Radius = 100.0f;
    }

    const float   FovRadians = ViewTransform.FOV;
    const float   Distance   = Radius / std::tan(FovRadians * 0.5f) * 1.25f;
    const FVector ViewDir    = FVector(-1.0f, -1.0f, -0.6f).Normalized();

    ViewTransform.ViewLocation = Center - ViewDir * Distance;
    ViewTransform.LookAt(Center);

    TargetLocation                        = ViewTransform.ViewLocation;
    LastAppliedCameraLocation             = ViewTransform.ViewLocation;
    bTargetLocationInitialized            = true;
    bLastAppliedCameraLocationInitialized = true;
}

bool FParticleSystemEditorViewportClient::IsMouseOverViewport() const
{
    if (!bIsRenderable || ViewportScreenRect.Width <= 0.0f || ViewportScreenRect.Height <= 0.0f)
    {
        return false;
    }

    ImVec2 MousePos = ImGui::GetMousePos();

    return MousePos.x >= ViewportScreenRect.X && MousePos.x <= ViewportScreenRect.X + ViewportScreenRect.Width &&
    MousePos.y >= ViewportScreenRect.Y && MousePos.y <= ViewportScreenRect.Y + ViewportScreenRect.Height;
}

void FParticleSystemEditorViewportClient::NotifyViewportResized(int32 NewWidth, int32 NewHeight)
{
    if (Viewport && NewHeight > 0)
    {
        ViewTransform.AspectRatio = static_cast<float>(NewWidth) / static_cast<float>(NewHeight);
    }
}

bool FParticleSystemEditorViewportClient::GetCameraView(FMinimalViewInfo& OutPOV) const
{
    OutPOV.Location    = ViewTransform.ViewLocation;
    OutPOV.Rotation    = ViewTransform.ViewRotation;
    OutPOV.FOV         = ViewTransform.FOV;
    OutPOV.AspectRatio = ViewTransform.AspectRatio;
    return true;
}

void FParticleSystemEditorViewportClient::Tick(float DeltaTime)
{
    SyncCameraSmoothingTarget();
    ApplySmoothedCameraLocation(DeltaTime);
    TickShortcuts();
    TickInput(DeltaTime);
}

void FParticleSystemEditorViewportClient::TickShortcuts()
{
    if (!FSlateApplication::Get().DoesClientOwnKeyboardInput(this))
    {
        return;
    }

    if (InputSystem::Get().GetKeyDown('F'))
    {
        ResetCameraToPreviewBounds();
    }
}

void FParticleSystemEditorViewportClient::TickInput(float DeltaTime)
{
    if (!FSlateApplication::Get().DoesClientOwnMouseInput(this))
    {
        return;
    }

    if (ImGui::GetIO().WantTextInput)
    {
        return;
    }

    FViewportCameraControlSettings& ControlSettings = FEditorSettings::Get().MeshEditorViewportSettings.CameraControls;

    InputSystem& Input = InputSystem::Get();

    FVector     LocalMove         = FVector::ZeroVector;
    float       WorldVerticalMove = 0.0f;
    const float CameraSpeed       = ControlSettings.MoveSpeed;

    if (Input.GetKey('W')) LocalMove.X += CameraSpeed;
    if (Input.GetKey('S')) LocalMove.X -= CameraSpeed;
    if (Input.GetKey('D')) LocalMove.Y += CameraSpeed;
    if (Input.GetKey('A')) LocalMove.Y -= CameraSpeed;
    if (Input.GetKey('Q')) WorldVerticalMove -= CameraSpeed;
    if (Input.GetKey('E')) WorldVerticalMove += CameraSpeed;

    const FVector Forward = ViewTransform.ViewRotation.GetForwardVector();
    const FVector Right   = ViewTransform.ViewRotation.GetRightVector();

    FVector DeltaMove = (Forward * LocalMove.X + Right * LocalMove.Y) * DeltaTime;
    DeltaMove.Z       += WorldVerticalMove * DeltaTime;
    TargetLocation    += DeltaMove;

    if (Input.GetKey(VK_RBUTTON))
    {
        const float MouseRotationSpeed = 0.15f * ControlSettings.RotationSpeed;
        const float DeltaYaw           = static_cast<float>(Input.MouseDeltaX()) * MouseRotationSpeed;
        const float DeltaPitch         = static_cast<float>(Input.MouseDeltaY()) * MouseRotationSpeed;
        ViewTransform.Rotate(DeltaYaw, DeltaPitch);
    }

    const float ScrollNotches = InputSystem::Get().GetScrollNotches();
    if (ScrollNotches != 0.0f)
    {
        if (InputSystem::Get().GetKey(VK_RBUTTON))
        {
            float& MoveSpeed = FEditorSettings::Get().MeshEditorViewportSettings.CameraControls.MoveSpeed;
            MoveSpeed        = ScrollNotches < 0.0f ? MoveSpeed * 0.9f : MoveSpeed * 1.1f;
            MoveSpeed        = Clamp(MoveSpeed, 0.001f, 1000.0f);
        }
        else if (ViewTransform.bIsOrtho)
        {
            const float NewWidth    = ViewTransform.OrthoZoom - ScrollNotches * ControlSettings.ZoomSpeed * DeltaTime;
            ViewTransform.OrthoZoom = Clamp(NewWidth, 0.1f, 1000.0f);
        }
        else
        {
            TargetLocation += ViewTransform.ViewRotation.GetForwardVector() * (ScrollNotches * ControlSettings.ZoomSpeed
                * 0.015f);
        }
    }
}

void FParticleSystemEditorViewportClient::SyncCameraSmoothingTarget()
{
    const FVector CurrentLocation        = ViewTransform.ViewLocation;
    const bool    bCameraMovedExternally = bLastAppliedCameraLocationInitialized && FVector::DistSquared(
        CurrentLocation,
        LastAppliedCameraLocation
    ) > 0.0001f;

    if (!bTargetLocationInitialized || bCameraMovedExternally)
    {
        TargetLocation             = CurrentLocation;
        bTargetLocationInitialized = true;
    }

    LastAppliedCameraLocation             = CurrentLocation;
    bLastAppliedCameraLocationInitialized = true;
}

void FParticleSystemEditorViewportClient::ApplySmoothedCameraLocation(float DeltaTime)
{
    const FVector CurrentLocation = ViewTransform.ViewLocation;
    const float   LerpAlpha       = Clamp(DeltaTime * SmoothLocationSpeed, 0.0f, 1.0f);
    const FVector NewLocation     = CurrentLocation + (TargetLocation - CurrentLocation) * LerpAlpha;

    ViewTransform.ViewLocation = NewLocation;

    LastAppliedCameraLocation             = NewLocation;
    bLastAppliedCameraLocationInitialized = true;
}
