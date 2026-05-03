#include "Viewport/GameViewportClient.h"

#include "Component/CameraComponent.h"
#include "Engine/Input/InputSystem.h"
#include "Math/MathUtils.h"
#include "Core/Log.h"

#include <windows.h>

DEFINE_CLASS(UGameViewportClient, UObject)

void UGameViewportClient::BeginGameSession(FViewport* InViewport)
{
	Viewport = InViewport;
	ResetInputState();
}

void UGameViewportClient::EndGameSession()
{
	SetInputPossessed(false);
	ResetInputState();
	bHasCursorClipRect = false;
	Viewport = nullptr;
}

void UGameViewportClient::ProcessInput(const FInputSystemSnapshot& Snapshot, float /*DeltaTime*/)
{
	// snapshot 저장은 호출이 들어온 매 프레임 항상 — possess off 인 동안에도 마지막 폴링값을
	// 보관해 둔다 (구 standalone ProcessInput 동작). possess 토글 시점의 "snapshot clear"
	// 는 SetInputPossessed 가 책임 (ProcessInput 호출이 끊겨도 즉시 비워짐).
	SetGameInputSnapshot(Snapshot);

	// 비활성 / 비포커스 — raw mouse / 커서 캡처 해제하고 입력 누적 리셋.
	if (!bInputPossessed || !Snapshot.bWindowFocused)
	{
		InputSystem::Get().SetUseRawMouse(false);
		SetCursorCaptured(false);
		ResetInputState();
		return;
	}

	// 활성 + 포커스 — raw mouse on, 커서 클립.
	InputSystem::Get().SetUseRawMouse(true);
	SetCursorCaptured(true);
}

void UGameViewportClient::SetInputPossessed(bool bPossessed)
{
	if (bInputPossessed == bPossessed)
	{
		return;
	}

	bInputPossessed = bPossessed;
	SetCursorCaptured(bPossessed);
	ResetInputState();

	// possess off 로 전환되는 순간 GameInputSnapshot 도 비워서 Lua 폴링이 즉시 빈 입력을 본다.
	// (ProcessInput 호출이 멈춘 뒤에도 이전 값이 남아있는 케이스 방지.)
	if (!bPossessed)
	{
		ClearGameInputSnapshot();
	}
}

void UGameViewportClient::SetCursorClipRect(const FRect& InViewportScreenRect)
{
	if (InViewportScreenRect.Width <= 1.0f || InViewportScreenRect.Height <= 1.0f)
	{
		bHasCursorClipRect = false;
		if (bCursorCaptured)
		{
			ApplyCursorClip();
		}
		return;
	}

	CursorClipClientRect.left = static_cast<LONG>(InViewportScreenRect.X);
	CursorClipClientRect.top = static_cast<LONG>(InViewportScreenRect.Y);
	CursorClipClientRect.right = static_cast<LONG>(InViewportScreenRect.X + InViewportScreenRect.Width);
	CursorClipClientRect.bottom = static_cast<LONG>(InViewportScreenRect.Y + InViewportScreenRect.Height);
	bHasCursorClipRect = CursorClipClientRect.right > CursorClipClientRect.left
		&& CursorClipClientRect.bottom > CursorClipClientRect.top;

	if (bCursorCaptured)
	{
		ApplyCursorClip();
	}
}

void UGameViewportClient::ResetInputState()
{
	InputSystem::Get().ResetMouseDelta();
	InputSystem::Get().ResetWheelDelta();
}

void UGameViewportClient::SetCursorCaptured(bool bCaptured)
{
	if (bCursorCaptured == bCaptured)
	{
		if (bCaptured)
		{
			ApplyCursorClip();
		}
		return;
	}

	bCursorCaptured = bCaptured;
	if (bCursorCaptured)
	{
		while (::ShowCursor(FALSE) >= 0) {}
		ApplyCursorClip();
		return;
	}

	while (::ShowCursor(TRUE) < 0) {}
	::ClipCursor(nullptr);
}

void UGameViewportClient::ApplyCursorClip()
{
	if (!OwnerHWnd)
	{
		return;
	}

	RECT ClientRect = {};
	if (bHasCursorClipRect)
	{
		ClientRect = CursorClipClientRect;
	}
	else if (!::GetClientRect(OwnerHWnd, &ClientRect))
	{
		return;
	}

	POINT TopLeft = { ClientRect.left, ClientRect.top };
	POINT BottomRight = { ClientRect.right, ClientRect.bottom };
	if (!::ClientToScreen(OwnerHWnd, &TopLeft) || !::ClientToScreen(OwnerHWnd, &BottomRight))
	{
		return;
	}

	RECT ScreenRect = { TopLeft.x, TopLeft.y, BottomRight.x, BottomRight.y };
	if (ScreenRect.right > ScreenRect.left && ScreenRect.bottom > ScreenRect.top)
	{
		::ClipCursor(&ScreenRect);
	}
}

void UGameViewportClient::SetGameInputSnapshot(const FInputSystemSnapshot& Snapshot)
{
	GameInputSnapshot = Snapshot;
	bHasGameInputSnapshot = true;
}

void UGameViewportClient::ClearGameInputSnapshot()
{
	GameInputSnapshot = FInputSystemSnapshot{};
	bHasGameInputSnapshot = false;
}
