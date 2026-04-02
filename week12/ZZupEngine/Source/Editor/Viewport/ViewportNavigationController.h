#pragma once

#include "Math/Vector.h"
#include "Math/Utils.h"
#include "Editor/Viewport/ViewportCamera.h"

class AActor;
class FSelectionManager;

class FViewportNavigationController
{
public:
	void SetCamera(FViewportCamera* InCamera) { ViewportCamera = InCamera; }
	void SetSelectionManager(FSelectionManager* InSelectionManager) { SelectionManager = InSelectionManager; }

	void Tick(float DeltaTime);

	void MoveForward(float Value, float DeltaTime);
	void MoveRight(float Value, float DeltaTime);
	void MoveUp(float Value, float DeltaTime);

	void AddYawInput(float Value);
	void AddPitchInput(float Value);

	void SetRotating(bool bInRotating);
	bool IsRotating() const { return bRotating; }

	void ModifyFOVorOrthoHeight(float Delta);
	void ModifyFOV(float DeltaFOV);
	void ModifyOrthoHeight(float DeltaHeight);

	void AdjustMoveSpeed(float Step) { MoveSpeed = MathUtil::Clamp<float>(MoveSpeed + Step, 10.0f, 2000.0f); }
	float GetMoveSpeed() const { return MoveSpeed; }
	void SetMoveSpeed(float InMoveSpeed) { MoveSpeed = MathUtil::Clamp<float>(InMoveSpeed, 10.0f, 2000.0f); }

	float GetRotationSpeed() const { return RotationSpeed; }
	void SetRotationSpeed(float InRotationSpeed)
	{
		RotationSpeed = MathUtil::Clamp<float>(InRotationSpeed, 0.01f, 10.0f);
	}

	// Orbit
	// void BeginOrbit(const FVector& InPivot);
	// void UpdateOrbitCamera();
	// void EndOrbit();
	// bool IsOrbiting() const { return bOrbiting; }

	// Dolly
	void Dolly(float Value);

	// Pan
	void BeginPanning();
	void EndPanning();
	void AddPanInput(float DeltaX, float DeltaY);
	bool IsPanning() const { return bPanning; }

	// 카메라 리셋 후 lerp 타겟 강제 초기화
	void ResetTargetLocation() { bHasTargetLocation = false; }

	float GetPanSpeed() const { return PanSpeed; }
	void SetPanSpeed(float InPanSpeed) { PanSpeed = MathUtil::Clamp<float>(InPanSpeed, 0.01f, 10.0f); }

	void TranslateWithGizmoDelta(const FVector& Delta);
	float GetGizmoFollowSpeedScale() const { return GizmoFollowSpeedScale; }
	void SetGizmoFollowSpeedScale(float InScale)
	{
		GizmoFollowSpeedScale = MathUtil::Clamp<float>(InScale, 0.01f, 5.0f);
	}

private:
	void UpdateCameraRotation();
	void EnsureTargetLocationInitialized();

private:
	FViewportCamera* ViewportCamera = nullptr;
	FSelectionManager* SelectionManager = nullptr;

	float MoveSpeed = 100.0f;
	float RotationSpeed = 0.3f;
	float LocationLerpSpeed = 12.0f;

	float Yaw = 0.0f;
	float Pitch = 0.0f;

	FVector TargetLocation = FVector::ZeroVector;
	bool bHasTargetLocation = false;

	bool bRotating = false;

	// Orbit
	bool bOrbiting = false;
	FVector OrbitPivot = FVector::ZeroVector;
	float OrbitRadius = 0.0f;
	float DefaultOrbitRadius = 300.0f;

	// Dolly
	float DollySpeed = 1.0f;
	float MinOrbitRadius = 10.0f;

	// Panning
	bool bPanning = false;
	float PanSpeed = 0.1f;

	float GizmoFollowSpeedScale = 0.5f;
};