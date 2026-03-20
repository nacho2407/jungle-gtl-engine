#pragma once
#include "Engine/Core/RayTypes.h"
#include "Object/ObjectFactory.h"
#include "World/SceneComponent.h"
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

enum class EProjectionMode { Perspective, Orthographic };

class UCamera : public USceneComponent {
public:
	DECLARE_CLASS(UCamera, USceneComponent)
	UCamera() = default;

	//	카메라의 상태는 카메라가 소유하고 적용합니다.
	void LookAt(const FVector& target);
	void SyncStateLookAt();
	void ApplyCameraState();
	void SetCameraState(const FCameraState& NewState);
	FCameraState& GetCameraState() { return CameraState; }
	const FCameraState& GetCameraState() const { return CameraState; }
	
	void OnResize(int w, int h);

	const FMatrix& GetViewMatrix();
	const FMatrix& GetProjectionMatrix();
	const FMatrix& GetViewProjection();
	void SetRelativeRotation(const FVector newrotation) override;

	float GetFOV() const;
	float GetNearPlane() const;
	float GetFarPlane() const;
	float GetOrthoWidth() const { return OrthoWidth; }
	
	EProjectionMode GetProjectionMode() const { return ProjectionMode;  }
	

	FRay DeprojectScreenToWorld(float MouseX, float MouseY, float ScreenWidth, float ScreenHeight);
private:
	float NearZ = 0.01f;
	float FarZ = 1000.0f;
	float FOV = M_PI / 3.0f; // In Radians
	//float AspectRatio = 16.0f / 9.0f;
	float AspectRatio = 1.f;
	float OrthoWidth = 10.0f;

	FMatrix CachedView;
	FMatrix CachedProjection;
	FMatrix CachedVP;

	EProjectionMode ProjectionMode = EProjectionMode::Perspective;

	bool bViewDirty = true;     // rebuild View if position/rotation changed
	bool bProjectionDirty = true;     // rebuild Projection if FOV/aspect changed

	FCameraState CameraState;

	//void RebuildAxis();
	void RebuildView();
	void RebuildProjection();
	void SetRelativeLocation(const FVector newlocaiton) override;

	void BuildLookAtRotation();

	void SetProjectionMode(EProjectionMode mode);

	void  SetFOV(float newFOV);

	
};