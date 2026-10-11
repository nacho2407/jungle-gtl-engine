#pragma once

#include "Object/Object.h"
#include "Slate/SWindow.h"
#include "Viewport/ViewportClient.h"
#include "Input/InputSystem.h"

#include "Source/Engine/Viewport/GameViewportClient.generated.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

class FViewport;
class UCameraComponent;

// UE의 UGameViewportClient 대응 — UObject + FViewportClient 다중상속.
// 게임 런타임 뷰포트를 담당 (Standalone / Editor PIE 양쪽 동일 인터페이스).
UCLASS()
class UGameViewportClient : public UObject, public FViewportClient
{
public:
	GENERATED_BODY()
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

	/**
	 * @brief 입력 차단 중 마우스 캡처 유지 여부를 설정합니다
	 *
	 * @param bCapture 입력 possession이 꺼진 동안에도 raw mouse와 cursor capture를 유지할지 여부
	 */
	void SetMouseCaptureWhileInputBlocked(bool bCapture);

	/**
	 * @brief 입력 차단 중 마우스 캡처 유지 여부
	 *
	 * @return 입력 차단 중 마우스 캡처 유지 플래그
	 */
	bool ShouldCaptureMouseWhileInputBlocked() const { return bCaptureMouseWhileInputBlocked; }

	// 게임 세션 진입/종료 — viewport attach + 입력 상태 리셋. PIE start/stop 또는
	// standalone 게임 시작/종료에서 호출.
	void BeginGameSession(FViewport* InViewport);
	void EndGameSession();

	void ResetInputState();

	// 매 프레임 입력 처리 — possess 가드 + GameInputSnapshot 갱신 + 커서/raw mouse 정책 적용.
	// 비활성/비포커스 시 snapshot 클리어 + raw mouse 해제 + 커서 풀어줌.
	void ProcessInput(const FInputSystemSnapshot& Snapshot, float DeltaTime);

	/**
	* @brief 현재 possession 상태를 반영한 게임 입력 스냅샷을 생성합니다.
	*
	* @return possession이 켜져 있고 창 포커스가 있으면 현재 입력 스냅샷, 아니면 빈 스냅샷
	*/
	FInputSystemSnapshot MakePossessedInputSnapshot() const;

	/**
	* @brief 현재 엔진의 GameViewportClient 기준 게임 입력 스냅샷을 생성합니다.
	*
	* @return GameViewportClient가 있으면 possession 가드가 적용된 스냅샷, 없으면 전역 입력 스냅샷
	*/
	static FInputSystemSnapshot MakeCurrentGameInputSnapshot();

	const FInputSystemSnapshot& GetGameInputSnapshot() const { return GameInputSnapshot; }

private:
	void ApplyBlockedInputMousePolicy();
	void SetCursorCaptured(bool bCaptured);
	void ApplyCursorClip();

	void SetGameInputSnapshot(const FInputSystemSnapshot& Snapshot);
	void ClearGameInputSnapshot();

	FViewport* Viewport = nullptr;
	HWND OwnerHWnd = nullptr;
	RECT CursorClipClientRect = {};
	bool bHasCursorClipRect = false;
	bool bInputPossessed = false;
	bool bCaptureMouseWhileInputBlocked = false;
	bool bCursorCaptured = false;

	FInputSystemSnapshot GameInputSnapshot{};
	bool bHasGameInputSnapshot = false;
};
