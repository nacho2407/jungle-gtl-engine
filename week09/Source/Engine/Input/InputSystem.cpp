#include "InputSystem.h"

#include <algorithm>
#include <cmath>
#include <Xinput.h>

#pragma comment(lib, "Xinput9_1_0.lib")

namespace
{
/**
 * @brief 아날로그 스틱의 한 축 값을 -1.0f ~ 1.0f 범위로 바꾸고, 값이 DeadZone 안에 있으면 0으로 처리합니다.
 *        스틱은 손을 떼고 있어도 완벽하게 0이 나오지 않을 수 있기 때문에 일정 범위를 DeadZone으로 설정해서 그 안의 작은 값은 무시합니다
 */
float NormalizeThumbAxis(SHORT Value, SHORT DeadZone)
{
    const int32 Raw = static_cast<int32>(Value);
    const int32 AbsValue = std::abs(Raw);

    if (AbsValue <= DeadZone)
    {
        return 0.0f;
    }

    const float Sign = Raw < 0 ? -1.0f : 1.0f;
    const float Normalized = static_cast<float>(AbsValue - DeadZone) / static_cast<float>(32767 - DeadZone);
    return std::clamp(Sign * Normalized, -1.0f, 1.0f);
}

/**
 * @brief 트리거 입력값을 0.0f ~ 1.0f 범위로 바꾸고, Threshold 이하의 작은 값은 0으로 처리합니다
 */
float NormalizeTrigger(BYTE Value, BYTE Threshold)
{
    if (Value <= Threshold)
    {
        return 0.0f;
    }

    return static_cast<float>(Value - Threshold) / static_cast<float>(255 - Threshold);
}

bool IsXInputButtonDown(const XINPUT_GAMEPAD& Pad, WORD Button)
{
    return (Pad.wButtons & Button) != 0;
}

WORD ToXInputButton(EGamepadButton Button)
{
    switch (Button)
    {
    case EGamepadButton::A:
        return XINPUT_GAMEPAD_A;
    case EGamepadButton::B:
        return XINPUT_GAMEPAD_B;
    case EGamepadButton::X:
        return XINPUT_GAMEPAD_X;
    case EGamepadButton::Y:
        return XINPUT_GAMEPAD_Y;
    case EGamepadButton::LeftShoulder:
        return XINPUT_GAMEPAD_LEFT_SHOULDER;
    case EGamepadButton::RightShoulder:
        return XINPUT_GAMEPAD_RIGHT_SHOULDER;
    case EGamepadButton::Back:
        return XINPUT_GAMEPAD_BACK;
    case EGamepadButton::Start:
        return XINPUT_GAMEPAD_START;
    case EGamepadButton::LeftThumb:
        return XINPUT_GAMEPAD_LEFT_THUMB;
    case EGamepadButton::RightThumb:
        return XINPUT_GAMEPAD_RIGHT_THUMB;
    case EGamepadButton::DPadUp:
        return XINPUT_GAMEPAD_DPAD_UP;
    case EGamepadButton::DPadDown:
        return XINPUT_GAMEPAD_DPAD_DOWN;
    case EGamepadButton::DPadLeft:
        return XINPUT_GAMEPAD_DPAD_LEFT;
    case EGamepadButton::DPadRight:
        return XINPUT_GAMEPAD_DPAD_RIGHT;
    default:
        return 0;
    }
}
} // namespace

void InputSystem::Tick(bool IsWindowFocused)
{
    PreviousSnapshot = CurrentSnapshot;
    CurrentSnapshot = {};

    if (!IsWindowFocused)
    {
        ClearInputOnFocusLost();
        return;
    }

    SampleVirtualKeys();
    SampleMouse();
    SampleWheel();
    SampleGamepads();
    UpdateModifiers();
}

void InputSystem::AddScrollDelta(int Delta)
{
    PendingWheelDelta += Delta;
}

void InputSystem::SampleVirtualKeys()
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

void InputSystem::SampleGamepads()
{
    for (int32 ControllerId = 0; ControllerId < MaxGamepadCount; ++ControllerId)
    {
        const FGamepadSnapshot& Prev = PreviousSnapshot.Gamepads[ControllerId];
        FGamepadSnapshot& Curr = CurrentSnapshot.Gamepads[ControllerId];

        XINPUT_STATE State{};
        const DWORD Result = XInputGetState(static_cast<DWORD>(ControllerId), &State);
        if (Result != ERROR_SUCCESS)
        {
            Curr.bConnected = false;
            continue;
        }

        Curr.bConnected = true;

        const XINPUT_GAMEPAD& Pad = State.Gamepad;

        Curr.LeftX = NormalizeThumbAxis(Pad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        Curr.LeftY = NormalizeThumbAxis(Pad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        Curr.RightX = NormalizeThumbAxis(Pad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
        Curr.RightY = NormalizeThumbAxis(Pad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
        Curr.LeftTrigger = NormalizeTrigger(Pad.bLeftTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
        Curr.RightTrigger = NormalizeTrigger(Pad.bRightTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD);

        for (int32 ButtonIndex = 0; ButtonIndex < static_cast<int32>(EGamepadButton::Count); ++ButtonIndex)
        {
            const EGamepadButton Button = static_cast<EGamepadButton>(ButtonIndex);
            const bool bWasDown = Prev.ButtonDown[ButtonIndex];
            const bool bIsDown = IsXInputButtonDown(Pad, ToXInputButton(Button));

            Curr.ButtonDown[ButtonIndex] = bIsDown;
            Curr.ButtonPressed[ButtonIndex] = bIsDown && !bWasDown;
            Curr.ButtonReleased[ButtonIndex] = !bIsDown && bWasDown;
        }
    }
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

	for (int32 ControllerId = 0; ControllerId < MaxGamepadCount; ++ControllerId)
    {
        CurrentSnapshot.Gamepads[ControllerId] = {};
    }
}
