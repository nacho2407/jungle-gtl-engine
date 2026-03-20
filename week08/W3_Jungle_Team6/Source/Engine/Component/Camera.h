#pragma once
#include "Engine/Core/RayTypes.h"
#include "Object/ObjectFactory.h"
#include "Component/SceneComponent.h"
#include "Math/Matrix.h"
#include "Math/Utils.h"
#include "Math/Vector.h"

struct FCameraState
{
	float FOV = M_PI / 3.0f;
	float AspectRatio = 16.0f / 9.0f;
	float NearZ = 0.1f;
	float FarZ = 1000.0f;
	float OrthoWidth = 10.0f;
	bool bIsOrthogonal = false;
};

class UCamera : public USceneComponent {
public:
	DECLARE_CLASS(UCamera, USceneComponent)
	UCamera() = default;

	void LookAt(const FVector& target);
	void ApplyCameraState();
	void SetCameraState(const FCameraState& NewState);
	FCameraState& GetCameraState() { return CameraState; }
	const FCameraState& GetCameraState() const { return CameraState; }

	void OnResize(int w, int h);

	const FMatrix& GetViewMatrix();
	const FMatrix& GetProjectionMatrix();
	const FMatrix& GetViewProjection();
	void SetRelativeRotation(const FVector newrotation) override;

	float GetFOV() const { return CameraState.FOV; }
	float GetNearPlane() const { return CameraState.NearZ; }
	float GetFarPlane() const { return CameraState.FarZ; }
	float GetOrthoWidth() const { return CameraState.OrthoWidth; }
	bool IsOrthogonal() const { return CameraState.bIsOrthogonal; }

	FRay DeprojectScreenToWorld(float MouseX, float MouseY, float ScreenWidth, float ScreenHeight);
private:
	FMatrix CachedView;
	FMatrix CachedProjection;
	FMatrix CachedVP;

	bool bViewDirty = true;
	bool bProjectionDirty = true;

	FCameraState CameraState;

	void RebuildView();
	void RebuildProjection();
	void SetRelativeLocation(const FVector newlocaiton) override;
};
