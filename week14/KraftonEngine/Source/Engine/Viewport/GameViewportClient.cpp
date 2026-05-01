#include "Viewport/GameViewportClient.h"

#include "Component/CameraComponent.h"
#include "Engine/Input/InputSystem.h"
#include "Math/MathUtils.h"
#include "Core/Log.h"

#include <windows.h>

DEFINE_CLASS(UGameViewportClient, UObject)

void UGameViewportClient::OnBeginPIE(FViewport* InViewport)
{
	Viewport = InViewport;
	ResetInputState();
}

void UGameViewportClient::OnEndPIE()
{
	SetPossessed(false);
	ResetInputState();
	bHasCursorClipRect = false;
	Viewport = nullptr;
}

bool UGameViewportClient::ProcessPIEInput(const FInputSystemSnapshot& Snapshot, float DeltaTime)
{
	if (!bPIEPossessedInputEnabled)
	{
		ClearGameInputSnapshot();
		return false;
	}

	SetGameInputSnapshot(Snapshot);
	return Tick(DeltaTime, Snapshot);
}

bool UGameViewportClient::ProcessInput(const FInputSystemSnapshot& Snapshot, float DeltaTime)
{
	SetGameInputSnapshot(Snapshot);
	return Tick(DeltaTime, Snapshot);
}

void UGameViewportClient::SetPIEPossessedInputEnabled(bool bEnabled)
{
	SetPossessed(bEnabled);
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

void UGameViewportClient::SetPossessed(bool bPossessed)
{
	if (bPIEPossessedInputEnabled == bPossessed)
	{
		return;
	}

	bPIEPossessedInputEnabled = bPossessed;
	SetCursorCaptured(bPossessed);
	ResetInputState();
}

void UGameViewportClient::ResetInputState()
{
	InputSystem::Get().ResetMouseDelta();
	InputSystem::Get().ResetWheelDelta();
}

bool UGameViewportClient::Tick(float DeltaTime, const FInputSystemSnapshot& Snapshot)
{
	if (!bPIEPossessedInputEnabled)
	{
		return false;
	}

	if (!Snapshot.bWindowFocused)
	{
		InputSystem::Get().SetUseRawMouse(false);
		SetCursorCaptured(false);
		ResetInputState();
		return false;
	}

	InputSystem::Get().SetUseRawMouse(true);
	SetCursorCaptured(true);

	return true;
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
