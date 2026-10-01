// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Component/CameraComponent.h"
#include "Viewport/Viewport.h"
#include <cmath>

void FSceneView::SetCameraInfo(const UCameraComponent* Camera)
{
    View           = Camera->GetViewMatrix();
    Proj           = Camera->GetProjectionMatrix();
    CameraPosition = Camera->GetWorldLocation();
    CameraForward  = Camera->GetForwardVector();
    CameraRight    = Camera->GetRightVector();
    CameraUp       = Camera->GetUpVector();
    bIsOrtho       = Camera->IsOrthogonal();
    OrthoWidth     = Camera->GetOrthoWidth();
    NearClip       = Camera->GetCameraState().NearZ;
    FarClip        = Camera->GetCameraState().FarZ;
    FOV            = Camera->GetFOV();
    ScreenEffects  = FCameraScreenEffectInfo{};
    FrustumVolume.UpdateFromMatrix(View * Proj);
}

void FSceneView::SetCameraInfo(const FCameraViewInfo& CameraViewInfo)
{
    const FVector F = CameraViewInfo.Rotation.GetForwardVector();
    const FVector R = CameraViewInfo.Rotation.GetRightVector();
    const FVector U = CameraViewInfo.Rotation.GetUpVector();
    const FVector E = CameraViewInfo.Location;
    const FCameraState& CameraState = CameraViewInfo.CameraState;

    View = FMatrix(
        R.X, U.X, F.X, 0,
        R.Y, U.Y, F.Y, 0,
        R.Z, U.Z, F.Z, 0,
        -E.Dot(R), -E.Dot(U), -E.Dot(F), 1);

    if (!CameraState.bIsOrthogonal)
    {
        Proj = FMatrix::MakePerspective(CameraState.FOV, CameraState.AspectRatio, CameraState.NearZ, CameraState.FarZ);
    }
    else
    {
        const float Width = CameraState.OrthoWidth;
        const float Height = Width / CameraState.AspectRatio;
        Proj = FMatrix::MakeOrthographic(Width, Height, CameraState.NearZ, CameraState.FarZ);
    }

    CameraPosition = E;
    CameraForward = F;
    CameraRight = R;
    CameraUp = U;
    bIsOrtho = CameraState.bIsOrthogonal;
    OrthoWidth = CameraState.OrthoWidth;
    NearClip = CameraState.NearZ;
    FarClip = CameraState.FarZ;
    FOV = CameraState.FOV;
    ScreenEffects = CameraViewInfo.ScreenEffects;
    FrustumVolume.UpdateFromMatrix(View * Proj);
}

void FSceneView::SetViewportInfo(const FViewport* VP)
{
    if (!VP)
    {
        ViewportWidth  = 0.0f;
        ViewportHeight = 0.0f;
        return;
    }

    ViewportWidth  = static_cast<float>(VP->GetWidth());
    ViewportHeight = static_cast<float>(VP->GetHeight());
}
