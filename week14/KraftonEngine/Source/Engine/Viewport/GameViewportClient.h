#pragma once

#include "Object/Object.h"
#include "UI/SWindow.h"
#include "Viewport/ViewportClient.h"
#include "Input/InputSystem.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

class FViewport;
class UCameraComponent;

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

	void SetPIEPossessedInputEnabled(bool bEnabled);
	bool IsPIEPossessedInputEnabled() const { return bPIEPossessedInputEnabled; }
	bool IsPossessed() const { return bPIEPossessedInputEnabled; }
	void SetPossessed(bool bPossessed);
	void OnBeginPIE(FViewport* InViewport);
	void OnEndPIE();
	void ResetInputState();
	bool Tick(float DeltaTime, const FInputSystemSnapshot& Snapshot);
	bool ProcessPIEInput(const FInputSystemSnapshot& Snapshot, float DeltaTime);

	// Standalone에서의 ProcessInput
	bool ProcessInput(const FInputSystemSnapshot& Snapshot, float DeltaTime);

	const FInputSystemSnapshot& GetGameInputSnapshot() const { return GameInputSnapshot; }

private:
	void SetCursorCaptured(bool bCaptured);
	void ApplyCursorClip();

	void SetGameInputSnapshot(const FInputSystemSnapshot& Snapshot);
	void ClearGameInputSnapshot();

	FViewport* Viewport = nullptr;
	HWND OwnerHWnd = nullptr;
	RECT CursorClipClientRect = {};
	bool bHasCursorClipRect = false;
	bool bPIEPossessedInputEnabled = false;
	bool bCursorCaptured = false;

	FInputSystemSnapshot GameInputSnapshot{};
	bool bHasGameInputSnapshot = false;
};
