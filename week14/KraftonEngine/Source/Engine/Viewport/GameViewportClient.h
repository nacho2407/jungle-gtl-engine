#pragma once

#include "Object/Object.h"
#include "UI/SWindow.h"
#include "Viewport/ViewportClient.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

class FViewport;
class UCameraComponent;
struct FInputSystemSnapshot;

struct FGameInputSettings
{
	float MoveSpeed = 10.0f;
	float SprintMultiplier = 2.5f;
	float LookSensitivity = 0.08f;
	float MinPitch = -89.0f;
	float MaxPitch = 89.0f;
};

// UE의 UGameViewportClient 대응 — UObject + FViewportClient 다중상속
// 게임 런타임 뷰포트를 담당 (PIE / Standalone)
class UGameViewportClient : public UObject, public FViewportClient
{
public:
	DECLARE_CLASS(UGameViewportClient, UObject)

	UGameViewportClient() = default;
	~UGameViewportClient() override = default;

	// FViewportClient overrides
	void Draw(FViewport* Viewport, float DeltaTime) override {}
	bool InputKey(int32 Key, bool bPressed) override { return false; }

	// Viewport 소유
	void SetViewport(FViewport* InViewport) { Viewport = InViewport; }
	FViewport* GetViewport() const { return Viewport; }
	void SetOwnerWindow(HWND InOwnerHWnd) { OwnerHWnd = InOwnerHWnd; }
	void SetCursorClipRect(const FRect& InViewportScreenRect);
	void SetDrivingCamera(UCameraComponent* InCamera) { Possess(InCamera); }
	UCameraComponent* GetDrivingCamera() const { return GetPossessedTarget(); }

	void SetPIEPossessedInputEnabled(bool bEnabled);
	bool IsPIEPossessedInputEnabled() const { return bPIEPossessedInputEnabled; }
	bool IsPossessed() const { return bPIEPossessedInputEnabled; }
	void SetPossessed(bool bPossessed);
	void OnBeginPIE(UCameraComponent* InitialTarget, FViewport* InViewport);
	void OnEndPIE();
	void ResetInputState();
	void Possess(UCameraComponent* TargetCamera);
	void UnPossess();
	UCameraComponent* GetPossessedTarget() const { return PossessedCamera; }
	bool HasPossessedTarget() const { return PossessedCamera != nullptr; }
	bool Tick(float DeltaTime, const FInputSystemSnapshot& Snapshot);
	bool ProcessPIEInput(const FInputSystemSnapshot& Snapshot, float DeltaTime);

private:
	bool ApplyInputToCameraOrActor(float DeltaTime, const FInputSystemSnapshot& Snapshot);
	bool ApplyMovementInput(float DeltaTime, const FInputSystemSnapshot& Snapshot);
	bool ApplyLookInput(const FInputSystemSnapshot& Snapshot);
	void SetCursorCaptured(bool bCaptured);
	void ApplyCursorClip();

	FViewport* Viewport = nullptr;
	HWND OwnerHWnd = nullptr;
	UCameraComponent* PossessedCamera = nullptr;
	FGameInputSettings InputSettings{};
	RECT CursorClipClientRect = {};
	bool bHasCursorClipRect = false;
	bool bPIEPossessedInputEnabled = false;
	bool bCursorCaptured = false;
};
