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

// UE의 UGameViewportClient 대응 — UObject + FViewportClient 다중상속.
// 게임 런타임 뷰포트를 담당 (Standalone / Editor PIE 양쪽 동일 인터페이스).
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

	// Input possess — 게임 입력(raw mouse + 커서 캡처 + InputSystem snapshot 라우팅) 활성/비활성 토글.
	// 표준 게임 세션과 PIE 양쪽에서 동일하게 사용.
	void SetInputPossessed(bool bPossessed);
	bool IsPossessed() const { return bInputPossessed; }

	// 게임 세션 진입/종료 — viewport attach + 입력 상태 리셋. PIE start/stop 또는
	// standalone 게임 시작/종료에서 호출.
	void BeginGameSession(FViewport* InViewport);
	void EndGameSession();

	void ResetInputState();

	// 매 프레임 입력 처리 — possess 가드 + GameInputSnapshot 갱신 + 커서/raw mouse 정책 적용.
	// 비활성/비포커스 시 snapshot 클리어 + raw mouse 해제 + 커서 풀어줌.
	void ProcessInput(const FInputSystemSnapshot& Snapshot, float DeltaTime);

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
	bool bInputPossessed = false;
	bool bCursorCaptured = false;

	FInputSystemSnapshot GameInputSnapshot{};
	bool bHasGameInputSnapshot = false;
};
