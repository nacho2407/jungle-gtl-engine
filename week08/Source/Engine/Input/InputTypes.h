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

	// TODO: Milestone 3에서 Gamepad 입력을 추가하면서 추가될 예정
    //GamepadLeftX,
    //GamepadLeftY,
    //GamepadRightX,
    //GamepadRightY,
    //GamepadLeftTrigger,
    //GamepadRightTrigger
};

/**
 * @brief 키 이벤트나 마우스 이벤트처럼 이산적인 이벤트와 달리, 매 프레임마다 변화량을 전달하는 연속적인 입력 이벤트
 */
struct FViewportAxisEvent
{
    EInputAxis Axis = EInputAxis::None;
    float Value = 0.0f;
    float DeltaTime = 0.0f;

	// TODO: 마찬가지로 Milestone 3에서 Gamepad 입력이 추가되면, 이 구분 축도 추가될 예정
	//int32 ContollerId = 0;

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
