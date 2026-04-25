#pragma once
#include "Engine/Core/RayTypes.h"
#include "Object/ObjectFactory.h"
#include "Component/SceneComponent.h"
#include "Math/Matrix.h"
#include "Math/MathUtils.h"
#include "Math/Vector.h"
#include "Collision/ConvexVolume.h"

struct FCameraState
{
	float FOV = 3.14159265358979f / 3.0f;
	float AspectRatio = 16.0f / 9.0f;
	float NearZ = 0.1f;
	float FarZ = 1000.0f;
	float OrthoWidth = 10.0f;
	bool bIsOrthogonal = false;
};

class UCameraComponent : public USceneComponent
{
public:
	DECLARE_CLASS(UCameraComponent, USceneComponent)

	UCameraComponent() = default;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	void LookAt(const FVector& Target);
	void SetCameraState(const FCameraState& NewState);
	const FCameraState& GetCameraState() const { return CameraState; }

	void SetFOV(float InFOV) { CameraState.FOV = InFOV; }
	void SetOrthoWidth(float InWidth) { CameraState.OrthoWidth = InWidth; }
	void SetOrthographic(bool bOrtho) { CameraState.bIsOrthogonal = bOrtho; }

	void OnResize(int32 Width, int32 Height);

	FMatrix GetViewMatrix() const;
	FMatrix GetProjectionMatrix() const;
	FMatrix GetViewProjectionMatrix() const;
	FConvexVolume GetConvexVolume() const;

	float GetFOV() const { return CameraState.FOV; }
	float GetNearPlane() const { return CameraState.NearZ; }
	float GetFarPlane() const { return CameraState.FarZ; }
	float GetOrthoWidth() const { return CameraState.OrthoWidth; }
	bool IsOrthogonal() const { return CameraState.bIsOrthogonal; }

	FRay DeprojectScreenToWorld(float MouseX, float MouseY, float ScreenWidth, float ScreenHeight);

private:
	FCameraState CameraState;
};
