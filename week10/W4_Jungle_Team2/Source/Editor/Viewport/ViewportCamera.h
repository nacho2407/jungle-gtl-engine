#pragma once

#include "Core/RayTypes.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"

enum class EViewportProjectionType
{
	Perspective,
	Orthographic
};

class FViewportCamera
{
public:
	FViewportCamera() = default;
	~FViewportCamera() = default;

	void SetLocation(const FVector& InLocation);
	void SetRotation(const FQuat& InRotation);
	void SetRotation(const FRotator& InRotation);

	const FVector& GetLocation() const { return Location; }
	const FQuat& GetRotation() const { return Rotation; }

	FVector GetForwardVector() const;
	FVector GetRightVector() const;
	FVector GetUpVector() const;

	FMatrix GetViewMatrix() const;
	FMatrix GetProjectionMatrix() const;
	FMatrix GetViewProjectionMatrix() const;
	FRay DeprojectScreenToWorld(float ScreenX, float ScreenY, float ScreenWidth, float ScreenHeight) const;

	void SetProjectionType(EViewportProjectionType InType);
	EViewportProjectionType GetProjectionType() const { return ProjectionType; }
	bool IsOrthographic() const { return ProjectionType == EViewportProjectionType::Orthographic; }

	void SetFOV(float InFOV);
	void SetNearPlane(float InNear);
	void SetFarPlane(float InFar);
	void SetOrthoHeight(float InHeight);
	void SetLookAt(const FVector &Target);

	float GetFOV() const { return FOV; }
	float GetNearPlane() const { return NearPlane; }
	float GetFarPlane() const { return FarPlane; }
	float GetOrthoHeight() const { return OrthoHeight; }

	void OnResize(uint32 InWidth, uint32 InHeight);

	uint32 GetWidth() const { return Width; }
	uint32 GetHeight() const { return Height; }
	float GetAspectRatio() const { return AspectRatio; }

private:
	void MarkViewDirty() { bIsViewDirty = true; }
	void MarkProjectionDirty() { bIsProjectionDirty = true; }

private:
	FVector Location = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;

	EViewportProjectionType ProjectionType = EViewportProjectionType::Perspective;

	uint32 Width = 1920;
	uint32 Height = 1080;
	float AspectRatio = 16.0f / 9.0f;

	// 기본값은 기존 CameraComponent와 비슷하게 60도
	float FOV = 3.14159265358979f / 3.0f;
	float NearPlane = 0.1f;
	float FarPlane = 2000.0f;

	// OrthoWidth 대신 높이를 기준으로 관리
	float OrthoHeight = 10.0f;

	mutable FMatrix CachedViewMatrix = FMatrix::Identity;
	mutable FMatrix CachedProjectionMatrix = FMatrix::Identity;
	mutable bool bIsViewDirty = true;
	mutable bool bIsProjectionDirty = true;
};
