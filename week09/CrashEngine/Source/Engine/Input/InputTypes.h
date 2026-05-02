#pragma once

#include <windows.h>

#include "Core/CoreTypes.h"

enum class EKeyEventType : uint8
{
    Pressed,
    Released,
    Repeat
};

enum class EPointerButton : uint8
{
    None,
    Left,
    Right,
    Middle,
    X1, // 마우스 옆에 있는 뒤로가기 버튼
    X2  // 마우스 옆에 있는 앞으로가기 버튼
};

enum class EPointerEventType : uint8
{
	Pressed,
	Released,
	Moved,
	Wheel
};

struct FInputModifiers
{
    bool bCtrl = false;
    bool bAlt = false;
    bool bShift = false;
};

enum class EGamepadButton : uint8
{
    A,
    B,
    X,
    Y,
    LeftShoulder,
    RightShoulder,
    Back,
    Start,
    LeftThumb,
    RightThumb,
    DPadUp,
    DPadDown,
    DPadLeft,
    DPadRight,
    Count
};

struct FGamepadSnapshot
{
    bool bConnected = false;

    bool ButtonDown[static_cast<int32>(EGamepadButton::Count)] = {};
    bool ButtonPressed[static_cast<int32>(EGamepadButton::Count)] = {};
    bool ButtonReleased[static_cast<int32>(EGamepadButton::Count)] = {};

    float LeftX = 0.0f;
    float LeftY = 0.0f;
    float RightX = 0.0f;
    float RightY = 0.0f;
    float LeftTrigger = 0.0f;
    float RightTrigger = 0.0f;
};

// 연결 가능한 최대 게임패드 개수. XInput 최대 지원 개수는 4개
constexpr int32 MaxGamepadCount = 4;

/**
 * @brief 한 프레임 동안 수집한 원본 입력 상태 묶음
 */
struct FInputSnapshot
{
    bool KeyDown[256] = {};
    bool KeyPressed[256] = {};
    bool KeyReleased[256] = {};

    POINT MouseScreenPos = { 0, 0 };
    POINT PrevMouseScreenPos = { 0, 0 };
    POINT MouseDelta = { 0, 0 };

    int WheelDelta = 0; // Win32 API에서 전달되는 휠 델타
    float WheelNotches = 0.0f; // WheelDelta를 휠이 몇 칸(노치) 움직였는지로 변환한 값. 관례적으로 한 노치는 120으로 계산

	// 드래그 관련 정보는 raw 입력 상태라기 보다 해석된 정보에 가깝기 때문에, InputSnapshot에 두지 않고 ViewportInputRouter쪽에서 처리합니다

	FGamepadSnapshot Gamepads[MaxGamepadCount];

    FInputModifiers Modifiers;
};

/*
 * Viewport client에게 전달하기 좋게 가공한 입력 메시지 구조체들
 */

struct FViewportKeyEvent
{
    int32 Key = 0;
    EKeyEventType Type = EKeyEventType::Pressed;
    FInputModifiers Modifiers;
};

enum class EInputAxis : uint8
{
    None,

    MouseX,
    MouseY,
    MouseWheel,

    GamepadLeftX,
    GamepadLeftY,
    GamepadRightX,
    GamepadRightY,
    GamepadLeftTrigger,
    GamepadRightTrigger
};

/**
 * @brief 키 이벤트나 마우스 이벤트처럼 이산적인 이벤트와 달리, 매 프레임마다 변화량을 전달하는 연속적인 입력 이벤트
 */
struct FViewportAxisEvent
{
    EInputAxis Axis = EInputAxis::None;
    float Value = 0.0f;
    float DeltaTime = 0.0f;

	int32 ControllerId = 0;

    FInputModifiers Modifiers;
};

struct FViewportPointerEvent
{
    EPointerButton Button = EPointerButton::None;
    EPointerEventType Type = EPointerEventType::Pressed;

    POINT ScreenPos = { 0, 0 };
    POINT ClientPos = { 0, 0 };
    POINT LocalPos = { 0, 0 };

    FInputModifiers Modifiers;
};
