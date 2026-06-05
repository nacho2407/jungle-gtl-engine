#include "Viewport/GameViewportClient.h"

#include "Component/Camera/CameraComponent.h"
#include "Engine/Input/InputSystem.h"
#include "Math/MathUtils.h"
#include "Runtime/Engine.h"
#include "UI/UIManager.h"
#include "Core/Logging/Log.h"

#include <windows.h>

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
	// Shutdown 경로에서는 ProcessInput 이 더 이상 안 돌아 — 커서 캡처/clip 을 명시적으로 해제.
	// 이걸 안 풀면 ::ShowCursor 카운터 음수 + ::ClipCursor 클립이 종료 후에도 남아 다른 앱
	// 까지 영향받음 (특히 ClipCursor 는 프로세스 종료 후에도 잔존하다가 다음 SetCursorPos
	// 까지 유지될 수 있다).
	SetCursorCaptured(false);
	Viewport = nullptr;
}

void UGameViewportClient::ProcessInput(const FInputSystemSnapshot& Snapshot, float /*DeltaTime*/)
{
	// 비포커스 — raw mouse / 커서 캡처 해제하고 입력 누적 리셋.
	if (!Snapshot.bWindowFocused)
	{
		ClearGameInputSnapshot();
		InputSystem::Get().SetUseRawMouse(false);
		SetCursorCaptured(false);
		ResetInputState();
		return;
	}

	// possess off — 게임 입력 라우팅이 꺼진 상태. 커서는 풀어준다 (메뉴 진입 직후 등).
	if (!bInputPossessed)
	{
		ClearGameInputSnapshot();
		InputSystem::Get().SetUseRawMouse(false);
		SetCursorCaptured(false);
		return;
	}

	// active game input snapshot 저장 — possess on 상태에서만 Lua/게임 입력용으로 보관.
	SetGameInputSnapshot(Snapshot);

	// possess on 이라도 UI widget 이 마우스를 요구하면 시스템 커서 보이고 raw mouse 해제.
	// 게임 입력 라우팅 (Lua 폴링) 은 그대로 — 일시정지/모달 케이스에서 게임 입력까지 끊고
	// 싶으면 SetInputPossessed(false) 를 별도 호출.
	if (UUIManager::Get().AnyViewportWidgetWantsMouse())
	{
		InputSystem::Get().SetUseRawMouse(false);
		SetCursorCaptured(false);
		return;
	}

	// possess on + 포커스 + UI 가 마우스 안 씀 — raw mouse on, 커서 캡처/클립.
	InputSystem::Get().SetUseRawMouse(true);
	SetCursorCaptured(true);
}

FInputSystemSnapshot UGameViewportClient::MakePossessedInputSnapshot() const
{
	const FInputSystemSnapshot Snapshot = InputSystem::Get().MakeSnapshot();

	// 게임 입력 possession이 풀렸거나 윈도우 포커스가 없으면 gameplay 소비자에게 빈 입력을 제공.
	if (!bInputPossessed || !Snapshot.bWindowFocused)
	{
		return FInputSystemSnapshot{};
	}

	return Snapshot;
}

FInputSystemSnapshot UGameViewportClient::MakeCurrentGameInputSnapshot()
{
	// PIE/Standalone처럼 GameViewportClient가 존재하는 경우는 항상 possession 정책을 따른다.
	if (GEngine)
	{
		if (UGameViewportClient* GameViewportClient = GEngine->GetGameViewportClient())
		{
			return GameViewportClient->MakePossessedInputSnapshot();
		}
	}

	// 에디터 preview 등 게임 viewport가 없는 경로는 기존 전역 입력 동작을 유지한다.
	return InputSystem::Get().MakeSnapshot();
}

void UGameViewportClient::SetInputPossessed(bool bPossessed)
{
	if (bInputPossessed == bPossessed)
	{
		return;
	}

	bInputPossessed = bPossessed;
	ResetInputState();

	// 커서 가시성/캡처는 ProcessInput 이 매 프레임 possess + UI WantsMouse 를 보고 결정.
	// 여기서는 게임 입력 라우팅만 토글한다.

	// possess off 로 전환되는 순간 GameInputSnapshot 도 비워서 Lua 폴링이 즉시 빈 입력을 본다.
	// (ProcessInput 호출이 멈춘 뒤에도 이전 값이 남아있는 케이스 방지.)
	if (!bPossessed)
	{
		ClearGameInputSnapshot();
		InputSystem::Get().SetUseRawMouse(false);
		SetCursorCaptured(false);
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
