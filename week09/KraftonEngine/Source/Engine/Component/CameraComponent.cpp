// 컴포넌트 영역의 세부 동작을 구현합니다.
#include "Component/CameraComponent.h"
#include "Object/ObjectFactory.h"
#include <cmath>

IMPLEMENT_CLASS(UCameraComponent, USceneComponent)

FMatrix UCameraComponent::GetViewMatrix() const
{
    UpdateWorldMatrix();

    auto F = GetForwardVector();
    auto R = GetRightVector();
    auto U = GetUpVector();
    auto E = GetWorldLocation();

    return FMatrix(
        R.X, U.X, F.X, 0,
        R.Y, U.Y, F.Y, 0,
        R.Z, U.Z, F.Z, 0,
        -E.Dot(R), -E.Dot(U), -E.Dot(F), 1);
}

FMatrix UCameraComponent::GetProjectionMatrix() const
{
    float Cot = 1.0f / tanf(CameraState.FOV * 0.5f);
    float N = CameraState.NearZ;
    float F = CameraState.FarZ;

    if (!CameraState.bIsOrthogonal)
    {
        // Reversed-Z perspective: near→1, far→0
        float Denom = N - F;
        return FMatrix(
            Cot / CameraState.AspectRatio, 0, 0, 0,
            0, Cot, 0, 0,
            0, 0, N / Denom, 1,
            0, 0, -(F * N) / Denom, 0);
    }
    else
    {
        // Reversed-Z orthographic: near→1, far→0
        float HalfW = CameraState.OrthoWidth * 0.5f;
        float HalfH = HalfW / CameraState.AspectRatio;
        float Denom = N - F;
        return FMatrix(
            1.0f / HalfW, 0, 0, 0,
            0, 1.0f / HalfH, 0, 0,
            0, 0, 1.0f / Denom, 0,
            0, 0, -F / Denom, 1);
    }
}

FMatrix UCameraComponent::GetViewProjectionMatrix() const
{
    return GetViewMatrix() * GetProjectionMatrix();
}

FConvexVolume UCameraComponent::GetConvexVolume() const
{
    FConvexVolume ConvexVolume;
    ConvexVolume.UpdateFromMatrix(GetViewMatrix() * GetProjectionMatrix());
    return ConvexVolume;
}

void UCameraComponent::LookAt(const FVector& Target)
{
    FVector Position = GetWorldLocation();
    FVector Diff = (Target - Position).Normalized();

    constexpr float Rad2Deg = 180.0f / 3.14159265358979f;

    FRotator LookRotation = GetRelativeRotation();
    LookRotation.Pitch = -asinf(Diff.Z) * Rad2Deg;

    if (fabsf(Diff.Z) < 0.999f)
    {
        LookRotation.Yaw = atan2f(Diff.Y, Diff.X) * Rad2Deg;
    }

    SetRelativeRotation(LookRotation);
}

void UCameraComponent::OnResize(int32 Width, int32 Height)
{
    CameraState.AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
}

void UCameraComponent::SetCameraState(const FCameraState& NewState)
{
    CameraState = NewState;
}

FRay UCameraComponent::DeprojectScreenToWorld(float MouseX, float MouseY, float ScreenWidth, float ScreenHeight)
{
    float NdcX = (2.0f * MouseX) / ScreenWidth - 1.0f;
    float NdcY = 1.0f - (2.0f * MouseY) / ScreenHeight;

    // Reversed-Z: near plane = 1, far plane = 0
    FVector NdcNear(NdcX, NdcY, 1.0f);
    FVector NdcFar(NdcX, NdcY, 0.0f);

    FMatrix ViewProj = GetViewMatrix() * GetProjectionMatrix();
    FMatrix InverseViewProjection = ViewProj.GetInverse();

    FVector WorldNear = InverseViewProjection.TransformPositionWithW(NdcNear);
    FVector WorldFar = InverseViewProjection.TransformPositionWithW(NdcFar);

    FRay Ray;
    Ray.Origin = WorldNear;

    FVector Dir = WorldFar - WorldNear;
    float Length = std::sqrt(Dir.X * Dir.X + Dir.Y * Dir.Y + Dir.Z * Dir.Z);
    Ray.Direction = (Length > 1e-4f) ? Dir / Length : FVector(1, 0, 0);

    return Ray;
}

void UCameraComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    USceneComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "FOV", EPropertyType::Float, &CameraState.FOV, 0.1f, 3.14f, 0.01f });
    OutProps.push_back({ "Near Z", EPropertyType::Float, &CameraState.NearZ, 0.01f, 100.0f, 0.01f });
    OutProps.push_back({ "Far Z", EPropertyType::Float, &CameraState.FarZ, 1.0f, 100000.0f, 10.0f });
    OutProps.push_back({ "Orthographic", EPropertyType::Bool, &CameraState.bIsOrthogonal });
    OutProps.push_back({ "Ortho Width", EPropertyType::Float, &CameraState.OrthoWidth, 0.1f, 1000.0f, 0.5f });
}
