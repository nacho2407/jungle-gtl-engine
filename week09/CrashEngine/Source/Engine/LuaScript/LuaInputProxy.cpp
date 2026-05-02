#include "LuaInputProxy.h"

#include <Windows.h>

#include "Input/InputSystem.h"
#include "Input/InputTypes.h"

namespace
{
/*
 * note: Lua 스크립트에서는 키보드 / 마우스 / 게임패드 버튼을 문자열 이름을 통해 입력받음
 */

int32 KeyNameToVK(const FString& KeyName)
{
    if (KeyName.size() == 1)
    {
        char C = KeyName[0];
        if (C >= 'a' && C <= 'z')
            C = static_cast<char>(C - 'a' + 'A');
        if ((C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9'))
        {
            return static_cast<int32>(C);
        }
    }

    static const TMap<FString, int32> Map = {
        { "Space", VK_SPACE },
        { "Enter", VK_RETURN },
        { "Escape", VK_ESCAPE },
        { "Left", VK_LEFT },
        { "Right", VK_RIGHT },
        { "Up", VK_UP },
        { "Down", VK_DOWN },
        { "Shift", VK_SHIFT },
        { "Ctrl", VK_CONTROL },
        { "Alt", VK_MENU },
        { "Tab", VK_TAB },
    };

    auto It = Map.find(KeyName);
    return It != Map.end() ? It->second : -1;
}

int32 MouseButtonNameToVK(const FString& ButtonName)
{
    static const TMap<FString, int32> Map = {
        { "LeftMouseButton", VK_LBUTTON },
        { "RightMouseButton", VK_RBUTTON },
        { "MiddleMouseButton", VK_MBUTTON },
        { "MouseX1", VK_XBUTTON1 },
        { "MouseX2", VK_XBUTTON2 },
    };

    auto It = Map.find(ButtonName);
    return It != Map.end() ? It->second : -1;
}

// 게임패드 버튼은 VK 코드 대신 EGamepadButton 인덱스를 반환
int32 GamepadButtonNameToIndex(const FString& ButtonName)
{
    static const TMap<FString, int32> Map = {
        { "A", static_cast<int32>(EGamepadButton::A) },
        { "B", static_cast<int32>(EGamepadButton::B) },
        { "X", static_cast<int32>(EGamepadButton::X) },
        { "Y", static_cast<int32>(EGamepadButton::Y) },
        { "LeftShoulder", static_cast<int32>(EGamepadButton::LeftShoulder) },
        { "RightShoulder", static_cast<int32>(EGamepadButton::RightShoulder) },
        { "Back", static_cast<int32>(EGamepadButton::Back) },
        { "Start", static_cast<int32>(EGamepadButton::Start) },
        { "LeftThumb", static_cast<int32>(EGamepadButton::LeftThumb) },
        { "RightThumb", static_cast<int32>(EGamepadButton::RightThumb) },
        { "DPadUp", static_cast<int32>(EGamepadButton::DPadUp) },
        { "DPadDown", static_cast<int32>(EGamepadButton::DPadDown) },
        { "DPadLeft", static_cast<int32>(EGamepadButton::DPadLeft) },
        { "DPadRight", static_cast<int32>(EGamepadButton::DPadRight) },
    };

    auto It = Map.find(ButtonName);
    return It != Map.end() ? It->second : -1;
}
} // namespace

bool FLuaInputProxy::IsKeyDown(const FString& KeyName) const
{
    const int32 VK = KeyNameToVK(KeyName);
    if (VK < 0 || VK >= 256)
    {
        return false;
    }

    return InputSystem::Get().GetSnapshot().KeyDown[VK];
}

bool FLuaInputProxy::WasKeyPressed(const FString& KeyName) const
{
    const int32 VK = KeyNameToVK(KeyName);
    if (VK < 0 || VK >= 256)
    {
        return false;
    }

    return InputSystem::Get().GetSnapshot().KeyPressed[VK];
}

bool FLuaInputProxy::WasKeyReleased(const FString& KeyName) const
{
    const int32 VK = KeyNameToVK(KeyName);
    if (VK < 0 || VK >= 256)
    {
        return false;
    }

    return InputSystem::Get().GetSnapshot().KeyReleased[VK];
}

bool FLuaInputProxy::IsMouseButtonDown(const FString& ButtonName) const
{
	const int32 VK = MouseButtonNameToVK(ButtonName);
	if (VK < 0 || VK >= 256)
	{
		return false;
	}

    return InputSystem::Get().GetSnapshot().KeyDown[VK];
}

bool FLuaInputProxy::WasMouseButtonPressed(const FString& ButtonName) const
{
	const int32 VK = MouseButtonNameToVK(ButtonName);
	if (VK < 0 || VK >= 256)
	{
		return false;
	}

    return InputSystem::Get().GetSnapshot().KeyPressed[VK];
}

bool FLuaInputProxy::WasMouseButtonReleased(const FString& ButtonName) const
{
    const int32 VK = MouseButtonNameToVK(ButtonName);
	if (VK < 0 || VK >= 256)
	{
		return false;
    }

	return InputSystem::Get().GetSnapshot().KeyReleased[VK];
}

bool FLuaInputProxy::IsGamepadConnected(int32 ControllerId) const
{
	if (ControllerId < 0 || ControllerId >= MaxGamepadCount)
	{
		return false;
	}

    return InputSystem::Get().GetSnapshot().Gamepads[ControllerId].bConnected;
}

bool FLuaInputProxy::IsGamepadButtonDown(const FString& ButtonName, int32 ControllerId) const
{
    if (ControllerId < 0 || ControllerId >= MaxGamepadCount)
    {
        return false;
    }

    const int32 ButtonIndex = GamepadButtonNameToIndex(ButtonName);
	if (ButtonIndex < 0 || ButtonIndex >= static_cast<int32>(EGamepadButton::Count))
	{
		return false;
    }

	return InputSystem::Get().GetSnapshot().Gamepads[ControllerId].ButtonDown[ButtonIndex];
}

bool FLuaInputProxy::WasGamepadButtonPressed(const FString& ButtonName, int32 ControllerId) const
{
    if (ControllerId < 0 || ControllerId >= MaxGamepadCount)
    {
        return false;
    }

    const int32 ButtonIndex = GamepadButtonNameToIndex(ButtonName);
    if (ButtonIndex < 0 || ButtonIndex >= static_cast<int32>(EGamepadButton::Count))
    {
        return false;
    }

    return InputSystem::Get().GetSnapshot().Gamepads[ControllerId].ButtonPressed[ButtonIndex];
}

bool FLuaInputProxy::WasGamepadButtonReleased(const FString& ButtonName, int32 ControllerId) const
{
    if (ControllerId < 0 || ControllerId >= MaxGamepadCount)
    {
        return false;
    }

    const int32 ButtonIndex = GamepadButtonNameToIndex(ButtonName);
    if (ButtonIndex < 0 || ButtonIndex >= static_cast<int32>(EGamepadButton::Count))
    {
        return false;
    }

    return InputSystem::Get().GetSnapshot().Gamepads[ControllerId].ButtonReleased[ButtonIndex];
}

float FLuaInputProxy::GetAxis(const FString& AxisName, int32 ControllerId) const
{
    const FInputSnapshot& Snapshot = InputSystem::Get().GetSnapshot();

    if (AxisName == "MouseX")
    {
        return static_cast<float>(Snapshot.MouseDelta.x);
    }

    if (AxisName == "MouseY")
    {
        return static_cast<float>(Snapshot.MouseDelta.y);
    }

    if (AxisName == "MouseWheel")
    {
        return Snapshot.WheelNotches;
    }

    if (ControllerId < 0 || ControllerId >= MaxGamepadCount)
    {
        return 0.0f;
    }

    const FGamepadSnapshot& Pad = Snapshot.Gamepads[ControllerId];

    if (!Pad.bConnected)
    {
        return 0.0f;
    }

    if (AxisName == "GamepadLeftX")
    {
        return Pad.LeftX;
    }

    if (AxisName == "GamepadLeftY")
    {
        return Pad.LeftY;
    }

    if (AxisName == "GamepadRightX")
    {
        return Pad.RightX;
    }

    if (AxisName == "GamepadRightY")
    {
        return Pad.RightY;
    }

    if (AxisName == "GamepadLeftTrigger")
    {
        return Pad.LeftTrigger;
    }

    if (AxisName == "GamepadRightTrigger")
    {
        return Pad.RightTrigger;
    }

    return 0.0f;
}

FString LuaKeyNameFromVK(int32 VK)
{
    if (VK >= 'A' && VK <= 'Z')
        return FString(1, static_cast<char>(VK));
    if (VK >= '0' && VK <= '9')
        return FString(1, static_cast<char>(VK));

    switch (VK)
    {
    case VK_SPACE:
        return "Space";
    case VK_RETURN:
        return "Enter";
    case VK_ESCAPE:
        return "Escape";
    case VK_LEFT:
        return "Left";
    case VK_RIGHT:
        return "Right";
    case VK_UP:
        return "Up";
    case VK_DOWN:
        return "Down";
    case VK_SHIFT:
        return "Shift";
    case VK_CONTROL:
        return "Ctrl";
    case VK_MENU:
        return "Alt";
    case VK_TAB:
        return "Tab";
    default:
        return "Unknown";
    }
}

FString LuaGamepadButtonNameFromIndex(int32 ButtonIndex)
{
    const EGamepadButton Button = static_cast<EGamepadButton>(ButtonIndex);

    switch (Button)
    {
    case EGamepadButton::A:
        return "A";

    case EGamepadButton::B:
        return "B";

    case EGamepadButton::X:
        return "X";

    case EGamepadButton::Y:
        return "Y";

    case EGamepadButton::LeftShoulder:
        return "LeftShoulder";

    case EGamepadButton::RightShoulder:
        return "RightShoulder";

    case EGamepadButton::Back:
        return "Back";

    case EGamepadButton::Start:
        return "Start";

    case EGamepadButton::LeftThumb:
        return "LeftThumb";

    case EGamepadButton::RightThumb:
        return "RightThumb";

    case EGamepadButton::DPadUp:
        return "DPadUp";

    case EGamepadButton::DPadDown:
        return "DPadDown";

    case EGamepadButton::DPadLeft:
        return "DPadLeft";

    case EGamepadButton::DPadRight:
        return "DPadRight";

    default:
        return "";
    }
}
