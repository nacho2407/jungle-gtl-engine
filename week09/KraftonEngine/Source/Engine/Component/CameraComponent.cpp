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
		-E.Dot(R), -E.Dot(U), -E.Dot(F), 1
	);
}

FMatrix UCameraComponent::GetProjectionMatrix() const
{
	float Cot = 1.0f / tanf(CameraState.FOV * 0.5f);
	float Denom = CameraState.FarZ - CameraState.NearZ;

	if (!CameraState.bIsOrthogonal) {
		return FMatrix(
			Cot / CameraState.AspectRatio, 0, 0, 0,
			0, Cot, 0, 0,
			0, 0, CameraState.FarZ / Denom, 1,
			0, 0, -(CameraState.FarZ * CameraState.NearZ) / Denom, 0
		);
	}
	else {
		float HalfW = CameraState.OrthoWidth * 0.5f;
		float HalfH = HalfW / CameraState.AspectRatio;
		return FMatrix(
			1.0f / HalfW, 0, 0, 0,
			0, 1.0f / HalfH, 0, 0,
			0, 0, 1.0f / Denom, 0,
			0, 0, -CameraState.NearZ / Denom, 1
		);
	}
}

void UCameraComponent::LookAt(const FVector& Target)
{
	FVector Position = GetWorldLocation();
	FVector Diff = (Target - Position).Normalized();

	constexpr float Rad2Deg = 180.0f / 3.14159265358979f;

	FRotator LookRotation = GetRelativeRotation();
	LookRotation.Pitch = -asinf(Diff.Z) * Rad2Deg;

	if (fabsf(Diff.Z) < 0.999f) {
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

FRay UCameraComponent::DeprojectScreenToWorld(float MouseX, float MouseY, float ScreenWidth, float ScreenHeight) {
	float NdcX = (2.0f * MouseX) / ScreenWidth - 1.0f;
	float NdcY = 1.0f - (2.0f * MouseY) / ScreenHeight;

	FVector NdcNear(NdcX, NdcY, 0.0f);
	FVector NdcFar(NdcX, NdcY, 1.0f);

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
	OutProps.push_back({ "FOV",         EPropertyType::Float, &CameraState.FOV, 0.1f,   3.14f,    0.01f });
	OutProps.push_back({ "Near Z",      EPropertyType::Float, &CameraState.NearZ, 0.01f,  100.0f,   0.01f });
	OutProps.push_back({ "Far Z",       EPropertyType::Float, &CameraState.FarZ, 1.0f,   100000.0f, 10.0f });
	OutProps.push_back({ "Orthographic",EPropertyType::Bool,  &CameraState.bIsOrthogonal});
	OutProps.push_back({ "Ortho Width", EPropertyType::Float, &CameraState.OrthoWidth, 0.1f,   1000.0f,  0.5f });
}
