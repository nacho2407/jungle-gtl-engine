#include "Viewport/GameViewportClient.h"

#include "Component/CameraComponent.h"
#include "Engine/Input/InputSystem.h"
#include "Math/MathUtils.h"

#include <windows.h>

DEFINE_CLASS(UGameViewportClient, UObject)

void UGameViewportClient::OnBeginPIE(UCameraComponent* InitialTarget, FViewport* InViewport)
{
	Viewport = InViewport;
	Possess(InitialTarget);
	ResetInputState();
}

void UGameViewportClient::OnEndPIE()
{
	SetPossessed(false);
	UnPossess();
	ResetInputState();
	bHasCursorClipRect = false;
	Viewport = nullptr;
}

bool UGameViewportClient::ProcessPIEInput(const FInputSystemSnapshot& Snapshot, float DeltaTime)
{
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

void UGameViewportClient::Possess(UCameraComponent* TargetCamera)
{
	if (PossessedCamera == TargetCamera)
	{
		return;
	}

	PossessedCamera = TargetCamera;
	ResetInputState();
}

void UGameViewportClient::UnPossess()
{
	PossessedCamera = nullptr;
	SetCursorCaptured(false);
	InputSystem::Get().SetUseRawMouse(false);
	ResetInputState();
}

void UGameViewportClient::ResetInputState()
{
	InputSystem::Get().ResetMouseDelta();
	InputSystem::Get().ResetWheelDelta();
}

bool UGameViewportClient::Tick(float DeltaTime, const FInputSystemSnapshot& Snapshot)
{
	if (!bPIEPossessedInputEnabled || !HasPossessedTarget())
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
	return ApplyInputToCameraOrActor(DeltaTime, Snapshot);
}

bool UGameViewportClient::ApplyInputToCameraOrActor(float DeltaTime, const FInputSystemSnapshot& Snapshot)
{
	if (!HasPossessedTarget())
	{
		return false;
	}

	const bool bMoved = ApplyMovementInput(DeltaTime, Snapshot);
	const bool bLooked = ApplyLookInput(Snapshot);
	return bMoved || bLooked;
}

bool UGameViewportClient::ApplyMovementInput(float DeltaTime, const FInputSystemSnapshot& Snapshot)
{
	if (!HasPossessedTarget())
	{
		return false;
	}

	if (!Snapshot.bGuiUsingKeyboard && !Snapshot.bGuiUsingTextInput)
	{
		FVector MoveInput(0.0f, 0.0f, 0.0f);
		if (Snapshot.IsDown('W')) MoveInput.X += 1.0f;
		if (Snapshot.IsDown('S')) MoveInput.X -= 1.0f;
		if (Snapshot.IsDown('A')) MoveInput.Y -= 1.0f;
		if (Snapshot.IsDown('D')) MoveInput.Y += 1.0f;
		if (Snapshot.IsDown('E') || Snapshot.IsDown(VK_SPACE)) MoveInput.Z += 1.0f;
		if (Snapshot.IsDown('Q') || Snapshot.IsDown(VK_CONTROL)) MoveInput.Z -= 1.0f;

		if (!MoveInput.IsNearlyZero())
		{
			MoveInput = MoveInput.Normalized();

			UCameraComponent* TargetCamera = GetPossessedTarget();
			FVector FlatForward = TargetCamera->GetForwardVector();
			FVector FlatRight = TargetCamera->GetRightVector();
			FlatForward.Z = 0.0f;
			FlatRight.Z = 0.0f;
			if (!FlatForward.IsNearlyZero())
			{
				FlatForward = FlatForward.Normalized();
			}
			if (!FlatRight.IsNearlyZero())
			{
				FlatRight = FlatRight.Normalized();
			}

			const float SafeDeltaTime = (DeltaTime > 0.0f) ? DeltaTime : (1.0f / 60.0f);
			const float SpeedBoost = Snapshot.IsDown(VK_SHIFT) ? InputSettings.SprintMultiplier : 1.0f;
			const FVector WorldDelta = (FlatForward * MoveInput.X + FlatRight * MoveInput.Y + FVector::UpVector * MoveInput.Z)
				* (InputSettings.MoveSpeed * SpeedBoost * SafeDeltaTime);
			TargetCamera->SetWorldLocation(TargetCamera->GetWorldLocation() + WorldDelta);
			return true;
		}
	}

	return false;
}

bool UGameViewportClient::ApplyLookInput(const FInputSystemSnapshot& Snapshot)
{
	if (!HasPossessedTarget())
	{
		return false;
	}

	// PIE possessed mode owns mouse look. If ImGui explicitly captures mouse, skip look
	// so UI interactions do not leak into the controller.
	if (!Snapshot.bGuiUsingMouse && (Snapshot.MouseDeltaX != 0 || Snapshot.MouseDeltaY != 0))
	{
		UCameraComponent* TargetCamera = GetPossessedTarget();
		FRotator Rotation = TargetCamera->GetRelativeRotation();
		Rotation.Yaw += static_cast<float>(Snapshot.MouseDeltaX) * InputSettings.LookSensitivity;
		Rotation.Pitch = Clamp(
			Rotation.Pitch + static_cast<float>(Snapshot.MouseDeltaY) * InputSettings.LookSensitivity,
			InputSettings.MinPitch,
			InputSettings.MaxPitch);
		Rotation.Roll = 0.0f;
		TargetCamera->SetRelativeRotation(Rotation);
		return true;
	}

	return false;
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
