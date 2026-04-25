#include "InputSystem.h"

void InputSystem::SetGuiCaptureState(bool bMouse, bool bKeyboard)
{
    GuiState.bUsingMouse = bMouse;
    GuiState.bUsingKeyboard = bKeyboard;
}

void InputSystem::Tick()
{
    PreviousSnapshot = CurrentSnapshot;
    CurrentSnapshot = {};

	// 에디터 창이 포커스된 상태가 아닐 때
    if (OwnerHWnd && GetForegroundWindow() != OwnerHWnd)
    {
        ClearInputOnFocusLost();
        return;
    }

    SampleKeyboard();
    SampleMouse();
    SampleWheel();
    UpdateModifiers();
}

void InputSystem::AddScrollDelta(int Delta)
{
    PendingWheelDelta += Delta;
}

void InputSystem::SampleKeyboard()
{
    for (int VK = 0; VK < 256; VK++)
	{
        const bool bWasDown = PreviousSnapshot.KeyDown[VK];
        const bool bIsDown = (GetAsyncKeyState(VK) & 0x8000) != 0;

        CurrentSnapshot.KeyDown[VK] = bIsDown;
        CurrentSnapshot.KeyPressed[VK] = bIsDown && !bWasDown;
        CurrentSnapshot.KeyReleased[VK] = !bIsDown && bWasDown;
    }
}

void InputSystem::SampleMouse()
{
    POINT CurrentPos = { 0, 0 };
    GetCursorPos(&CurrentPos);

    CurrentSnapshot.MouseScreenPos = CurrentPos;

    if (!bHasMouseSample)
    {
        CurrentSnapshot.PrevMouseScreenPos = CurrentPos;
        CurrentSnapshot.MouseDelta = { 0, 0 };
        bHasMouseSample = true;
    }
	else
    {
        CurrentSnapshot.PrevMouseScreenPos = PreviousSnapshot.MouseScreenPos;
        CurrentSnapshot.MouseDelta.x = CurrentPos.x - PreviousSnapshot.MouseScreenPos.x;
        CurrentSnapshot.MouseDelta.y = CurrentPos.y - PreviousSnapshot.MouseScreenPos.y;
	}
}

void InputSystem::SampleWheel()
{
    CurrentSnapshot.WheelDelta = PendingWheelDelta;
    CurrentSnapshot.WheelNotches = static_cast<float>(PendingWheelDelta) / static_cast<float>(WHEEL_DELTA);

    PendingWheelDelta = 0;
}

void InputSystem::UpdateModifiers()
{
    CurrentSnapshot.Modifiers.bCtrl = CurrentSnapshot.KeyDown[VK_CONTROL];
    CurrentSnapshot.Modifiers.bAlt = CurrentSnapshot.KeyDown[VK_MENU];
    CurrentSnapshot.Modifiers.bShift = CurrentSnapshot.KeyDown[VK_SHIFT];
}

void InputSystem::ClearInputOnFocusLost()
{
    for (int VK = 0; VK < 256; VK++)
    {
        const bool bWasDown = PreviousSnapshot.KeyDown[VK];

        CurrentSnapshot.KeyDown[VK] = false;
        CurrentSnapshot.KeyPressed[VK] = false;
        CurrentSnapshot.KeyReleased[VK] = bWasDown;
    }

    POINT CurrentPos = { 0, 0 };
    GetCursorPos(&CurrentPos);

    CurrentSnapshot.MouseScreenPos = CurrentPos;
    CurrentSnapshot.PrevMouseScreenPos = CurrentPos;
    CurrentSnapshot.MouseDelta = { 0, 0 };

    CurrentSnapshot.WheelDelta = 0;
    CurrentSnapshot.WheelNotches = 0.0f;

    PendingWheelDelta = 0;

    bHasMouseSample = true;
}
