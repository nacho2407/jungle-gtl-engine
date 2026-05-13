#pragma once
#include "Engine/Geometry/Ray.h"
#include "Object/ObjectFactory.h"
#include "Component/SceneComponent.h"
#include "Math/Matrix.h"
#include "Math/Utils.h"
#include "Math/Vector.h"
#include "Math/Color.h"

// 렌더 전용 구조체
struct FCameraState
{
	float FOV = 3.14159265358979f / 3.0f;
	float AspectRatio = 16.0f / 9.0f;
	float NearZ = 0.1f;
	float FarZ = 1000.0f;
	float OrthoWidth = 10.0f;
	bool bIsOrthogonal = false;
};

struct FCameraPostProcessSettings
{
	bool bVignetteEnabled = false;
	float VignetteIntensity = 0.0f;
	float VignetteRadius = 0.75f;
	float VignetteSmoothness = 0.35f;
	FColor VignetteColor = FColor::Black();
};

// PlayerCameraManager 의 연산 결과를 담는 구조체
// 지금은 CameraState 와 대부분 겹치긴 하지만, 다른 레이어라고 생각하여 분리
struct FMinimalViewInfo
{
    float FOV = 3.14159265358979f / 3.0f;
    float AspectRatio = 16.0f / 9.0f;
    float NearZ = 0.1f;
    float FarZ = 1000.0f;
    float OrthoWidth = 10.0f;
    bool bIsOrthogonal = false;
    FCameraPostProcessSettings PostProcessSettings;

    FVector Location = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
};

class UCameraComponent : public USceneComponent
{
public:
	DECLARE_CLASS(UCameraComponent, USceneComponent)

	UCameraComponent() = default;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	virtual void Serialize(FArchive& Ar) override;

	void LookAt(const FVector& Target);
	void SetCameraState(const FCameraState& NewState);
	const FCameraState& GetCameraState() const { return CameraState; }

	void SetFOV(float InFOV) { CameraState.FOV = InFOV; }
	void SetOrthoWidth(float InWidth) { CameraState.OrthoWidth = InWidth; }
	void SetOrthographic(bool bOrtho) { CameraState.bIsOrthogonal = bOrtho; }
	void SetVignette(float Intensity, float Radius = 0.75f, float Smoothness = 0.35f, const FColor& Color = FColor::Black());
	void ClearVignette();
	const FCameraPostProcessSettings& GetPostProcessSettings() const { return PostProcessSettings; }

	void OnResize(int32 Width, int32 Height);

	FMatrix GetViewMatrix() const;
	FMatrix GetProjectionMatrix() const;

	float GetFOV() const { return CameraState.FOV; }
	float GetNearPlane() const { return CameraState.NearZ; }
	float GetFarPlane() const { return CameraState.FarZ; }
	float GetOrthoWidth() const { return CameraState.OrthoWidth; }
	bool IsOrthogonal() const { return CameraState.bIsOrthogonal; }

	FRay DeprojectScreenToWorld(float MouseX, float MouseY, float ScreenWidth, float ScreenHeight);

public:
	//	Unreal-style editor camera helpers
	void AddYawInput(float DeltaYawDegrees);
	void AddPitchInput(float DeltaPitchDegrees);

	void MoveForward(float Distance);
	void MoveRight(float Distance);
	void MoveUp(float Distance);

	float GetPitchDegrees() const;
	float GetYawDegrees() const;

	FVector GetForwardVector() const;
	FVector GetRightVector() const;
	FVector GetUpVector() const;

	// DeltaTime 기반 업데이트 가능성 열어둠
    void GetCameraView(float DeltaTime, FMinimalViewInfo& OutView) const;

private:
	void SetViewRotationDegrees(float PitchDegrees, float YawDegrees);

private:
	FCameraState CameraState;
	FCameraPostProcessSettings PostProcessSettings;
};
