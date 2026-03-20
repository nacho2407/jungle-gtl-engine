#include "Engine/Scene/Camera.h"
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
	// GetViewMatrix/GetProjectionMatrix handle their own dirty flags now
	CachedVP = GetViewMatrix() * GetProjectionMatrix();
	return CachedVP;
}

void UCamera::BuildLookAtRotation() {
	//FVector diff = (CameraState.InitLook - CameraState.Location).Normalized();

	//RelativeRotation.X = asinf(diff.Y);
	//RelativeRotation.Y = atan2f(diff.X, diff.Z) * (180.0f / M_PI);
	//RelativeRotation.Z = 0.0f;

	//CameraState.Rotation = RelativeRotation;
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
	AspectRatio = ((float)w) / ((float)h);
	CameraState.AspectRatio = AspectRatio;
	bProjectionDirty = true;
}

void UCamera::SetFOV(float newFOV) {
	FOV = newFOV;
	bProjectionDirty = true;
}

float UCamera::GetFOV() const {
	return FOV;
}

float UCamera::GetNearPlane() const {
	return NearZ;
}

float UCamera::GetFarPlane() const {
	return FarZ;
}

void UCamera::SyncStateLookAt()
{
	//BuildLookAtRotation();
}

void UCamera::ApplyCameraState()
{
	SetProjectionMode(CameraState.bIsOrthogonal ? EProjectionMode::Orthographic : EProjectionMode::Perspective);
	SetFOV(CameraState.FOV);

	AspectRatio = CameraState.AspectRatio;
	NearZ = CameraState.NearZ;
	FarZ = CameraState.FarZ;
	OrthoWidth = CameraState.OrthoWidth;
	bProjectionDirty = true;

	//SetWorldLocation(CameraState.Location);
	//SetRelativeRotation(CameraState.Rotation);
}

//	Camera 상태 갱신을 이로 통일
void UCamera::SetCameraState(const FCameraState& NewState)
{
	CameraState = NewState;
	ApplyCameraState();
}

void UCamera::RebuildView() {
	auto F = GetForwardVector(); // X (Forward)
	auto R = GetRightVector();   // Y (Right)
	auto U = GetUpVector();      // Z (Up)
	auto E = GetWorldLocation(); // Eye (Position)

	// [Z-up View Matrix]
	// Row-Major 기준이며, 카메라 좌표계(오른쪽, 위, 앞) 순서로 배치
	FMatrix mat(
		R.X, U.X, F.X, 0,
		R.Y, U.Y, F.Y, 0,
		R.Z, U.Z, F.Z, 0,
		-E.Dot(R), -E.Dot(U), -E.Dot(F), 1
	);

	CachedView = mat;
}


void UCamera::SetProjectionMode(EProjectionMode mode) {
	ProjectionMode = mode;

	bProjectionDirty = true;
}

void UCamera::RebuildProjection() {
	float cot = 1.0f / tanf(FOV * 0.5f);
	float denom = FarZ - NearZ;

	if (ProjectionMode == EProjectionMode::Perspective) {
		// [Z-up 전용 Perspective Matrix]
		// 렌더러가 Y-up(DirectX 표준)을 기대한다면 아래와 같이 매핑되어야 합니다.
		FMatrix mat(
			cot / AspectRatio, 0, 0, 0,
			0, cot, 0, 0,
			0, 0, FarZ / denom, 1, // Depth 축
			0, 0, -(FarZ * NearZ) / denom, 0
		);
		CachedProjection = mat;
	}
	else if (ProjectionMode == EProjectionMode::Orthographic) {
		float halfW = OrthoWidth * 0.5f;
		float halfH = halfW / AspectRatio;
		FMatrix mat(
			1.0f / halfW, 0, 0, 0,
			0, 1.0f / halfH, 0, 0,
			0, 0, 1.0f / denom, 0,
			0, 0, -NearZ / denom, 1
		);
		CachedProjection = mat;
	}
}

FRay UCamera::DeprojectScreenToWorld(float MouseX, float MouseY, float ScreenWidth, float ScreenHeight) {
	// Convert screen space to NDC
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
