#include "Actor/ACamera.h"

ACamera::ACamera() : AActor()
{
    auto MainCamera = NewObject<UCameraComponent>("CameraComponent");
    // 카메라가 바라보는 월드 수정 
    MainCamera->SetRelativeLocation(FVector(2.0f, 4.0f, -7.0f));
    MainCamera->SetRelativeRotation(FVector(0.3f, 0.0f, 0.0f)); // Pitch Yaw Roll
    MainCamera->SetFieldOfView(39.6f);
    MainCamera->SetNearClip(0.1f);
    MainCamera->SetFarClip(1000.0f);

    AddComponent(MainCamera);
    SetRootComponent(MainCamera);
    CameraComponent = MainCamera;
}

ACamera::~ACamera()
{
}

UCameraComponent* ACamera::GetCameraComponent() const
{
    check(CameraComponent)
    return CameraComponent;
}

void ACamera::SetAspectRatio(float Ratio)
{
    CameraComponent->SetAspectRatio(Ratio);
}
