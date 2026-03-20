#include "Component/Camera.h"
#include <cmath>

DEFINE_CLASS(UCamera, USceneComponent)
REGISTER_FACTORY(UCamera)

void UCamera::SetRelativeLocation(const FVector newlocation) {
	USceneComponent::SetRelativeLocation(newlocation);
	bViewDirty = true;
}

void UCamera::SetRelativeRotation(const FVector newrotation) {
	USceneComponent::SetRelativeRotation(newrotation);
	bViewDirty = true;
}

const FMatrix& UCamera::GetViewMatrix() {
	if (bUpdateFlag || bViewDirty) {
		RebuildView();
		bViewDirty = false;
		bUpdateFlag = false;
	}
	return CachedView;
}

const FMatrix& UCamera::GetProjectionMatrix() {
	if (bUpdateFlag || bProjectionDirty) {
		RebuildProjection();
		bProjectionDirty = false;
	}
	return CachedProjection;
}

const FMatrix& UCamera::GetViewProjection() {
	CachedVP = GetViewMatrix() * GetProjectionMatrix();
	return CachedVP;
}

void UCamera::LookAt(const FVector& target) {
	FVector Position = GetWorldLocation();
	FVector diff = (target - Position).Normalized();

	const float Rad2Deg = 180.0f / 3.1415926535f;

	RelativeRotation.Y = -1 * asinf(diff.Z) * Rad2Deg;

	if (fabsf(diff.Z) < 0.999f) {
		RelativeRotation.Z = atan2f(diff.Y, diff.X) * Rad2Deg;
	}

	SetRelativeRotation(RelativeRotation);
	bViewDirty = true;
}

void UCamera::OnResize(int w, int h) {
	CameraState.AspectRatio = static_cast<float>(w) / static_cast<float>(h);
	bProjectionDirty = true;
}

void UCamera::ApplyCameraState()
{
	bProjectionDirty = true;
}

void UCamera::SetCameraState(const FCameraState& NewState)
{
	CameraState = NewState;
	ApplyCameraState();
}

void UCamera::RebuildView() {
	auto F = GetForwardVector();
	auto R = GetRightVector();
	auto U = GetUpVector();
	auto E = GetWorldLocation();

	FMatrix mat(
		R.X, U.X, F.X, 0,
		R.Y, U.Y, F.Y, 0,
		R.Z, U.Z, F.Z, 0,
		-E.Dot(R), -E.Dot(U), -E.Dot(F), 1
	);

	CachedView = mat;
}

void UCamera::RebuildProjection() {
	float cot = 1.0f / tanf(CameraState.FOV * 0.5f);
	float denom = CameraState.FarZ - CameraState.NearZ;

	if (!CameraState.bIsOrthogonal) {
		FMatrix mat(
			cot / CameraState.AspectRatio, 0, 0, 0,
			0, cot, 0, 0,
			0, 0, CameraState.FarZ / denom, 1,
			0, 0, -(CameraState.FarZ * CameraState.NearZ) / denom, 0
		);
		CachedProjection = mat;
	}
	else {
		float halfW = CameraState.OrthoWidth * 0.5f;
		float halfH = halfW / CameraState.AspectRatio;
		FMatrix mat(
			1.0f / halfW, 0, 0, 0,
			0, 1.0f / halfH, 0, 0,
			0, 0, 1.0f / denom, 0,
			0, 0, -CameraState.NearZ / denom, 1
		);
		CachedProjection = mat;
	}
}

FRay UCamera::DeprojectScreenToWorld(float MouseX, float MouseY, float ScreenWidth, float ScreenHeight) {
	float ndcX = (2.0f * MouseX) / ScreenWidth - 1.0f;
	float ndcY = 1.0f - (2.0f * MouseY) / ScreenHeight;

	FVector ndcNear(ndcX, ndcY, 0.0f);
	FVector ndcFar(ndcX, ndcY, 1.0f);

	FMatrix viewProj = GetViewMatrix() * GetProjectionMatrix();
	FMatrix inverseViewProjection = viewProj.GetInverse();

	FVector worldNear = inverseViewProjection.TransformPositionWithW(ndcNear);
	FVector worldFar = inverseViewProjection.TransformPositionWithW(ndcFar);

	FRay ray;
	ray.Origin = worldNear;

	FVector dir = worldFar - worldNear;
	float length = std::sqrt(dir.X * dir.X + dir.Y * dir.Y + dir.Z * dir.Z);
	ray.Direction = (length > 1e-4f) ? dir / length : FVector(1, 0, 0);

	return ray;
}
