#include "Engine/Input/InputSystem.h"
#include "Core/Logging/Log.h"

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif

#include <algorithm>
#include <cmath>
#include <dinput.h>
#include <Xinput.h>

namespace
{
    constexpr float GAMEPAD_AXIS_DOWN_THRESHOLD = 0.0001f;
    constexpr float DIRECT_INPUT_AXIS_DEAD_ZONE = 0.18f;
    constexpr int DIRECT_INPUT_RETRY_FRAME_COUNT = 120;
    constexpr BYTE DIRECT_INPUT_BUTTON_DOWN = 0x80;
    constexpr LONG DIRECT_INPUT_AXIS_MIN = -32768;
    constexpr LONG DIRECT_INPUT_AXIS_MAX = 32767;

    const WORD GGamepadButtonMasks[InputCodes::GamepadButtonCount] =
    {
        XINPUT_GAMEPAD_A,
        XINPUT_GAMEPAD_B,
        XINPUT_GAMEPAD_X,
        XINPUT_GAMEPAD_Y,
        XINPUT_GAMEPAD_LEFT_SHOULDER,
        XINPUT_GAMEPAD_RIGHT_SHOULDER,
        XINPUT_GAMEPAD_BACK,
        XINPUT_GAMEPAD_START,
        XINPUT_GAMEPAD_LEFT_THUMB,
        XINPUT_GAMEPAD_RIGHT_THUMB,
        XINPUT_GAMEPAD_DPAD_UP,
        XINPUT_GAMEPAD_DPAD_DOWN,
        XINPUT_GAMEPAD_DPAD_LEFT,
        XINPUT_GAMEPAD_DPAD_RIGHT,
        0,
        0,
    };

    /**
    * @brief 유효한 XInput 게임패드 인덱스인지 반환합니다.
    */
    bool IsValidGamepadIndex(int GamepadIndex)
    {
        return GamepadIndex >= 0 && GamepadIndex < InputCodes::MaxGamepads;
    }

    int ResolveGamepadIndex(int GamepadIndex, int PrimaryGamepadIndex)
    {
        return GamepadIndex < 0 ? PrimaryGamepadIndex : GamepadIndex;
    }

    /**
    * @brief XInput 스틱 값을 dead zone 보정 후 -1.0~1.0으로 정규화합니다.
    */
    float NormalizeStickAxis(SHORT Value, SHORT DeadZone)
    {
        // dead zone 내부 값 제거
        const int AbsValue = std::abs(static_cast<int>(Value));
        if (AbsValue <= DeadZone)
        {
            return 0.0f;
        }

        // 양수/음수 최대치가 32767/-32768로 달라서 각 방향별 분모를 분리
        const float Normalized = Value > 0
            ? static_cast<float>(Value - DeadZone) / static_cast<float>(32767 - DeadZone)
            : static_cast<float>(Value + DeadZone) / static_cast<float>(32768 - DeadZone);

        if (Normalized > 1.0f)
        {
            return 1.0f;
        }
        if (Normalized < -1.0f)
        {
            return -1.0f;
        }
        return Normalized;
    }

    /**
    * @brief XInput trigger 값을 threshold 보정 후 0.0~1.0으로 정규화합니다.
    */
    float NormalizeTriggerAxis(BYTE Value)
    {
        // XInput 기본 trigger threshold 이하 값 제거
        if (Value <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
        {
            return 0.0f;
        }

        return static_cast<float>(Value - XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
            / static_cast<float>(255 - XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
    }

    bool IsDirectInputButtonDown(const DIJOYSTATE2& State, int ButtonIndex)
    {
        return ButtonIndex >= 0
            && ButtonIndex < 128
            && (State.rgbButtons[ButtonIndex] & DIRECT_INPUT_BUTTON_DOWN) != 0;
    }

    float NormalizeDirectInputAxis(LONG Value, bool bInvert)
    {
        float Normalized = Value < 0
            ? static_cast<float>(Value) / static_cast<float>(-DIRECT_INPUT_AXIS_MIN)
            : static_cast<float>(Value) / static_cast<float>(DIRECT_INPUT_AXIS_MAX);

        Normalized = std::clamp(Normalized, -1.0f, 1.0f);
        if (bInvert)
        {
            Normalized = -Normalized;
        }

        const float AbsValue = std::fabs(Normalized);
        if (AbsValue <= DIRECT_INPUT_AXIS_DEAD_ZONE)
        {
            return 0.0f;
        }

        const float Sign = Normalized >= 0.0f ? 1.0f : -1.0f;
        return Sign * ((AbsValue - DIRECT_INPUT_AXIS_DEAD_ZONE) / (1.0f - DIRECT_INPUT_AXIS_DEAD_ZONE));
    }

    float PickLargerMagnitude(float A, float B)
    {
        return std::fabs(A) >= std::fabs(B) ? A : B;
    }

    BOOL CALLBACK EnumDirectInputAxesCallback(const DIDEVICEOBJECTINSTANCEW* Object, void* Context)
    {
        IDirectInputDevice8W* Device = static_cast<IDirectInputDevice8W*>(Context);
        if (!Device)
        {
            return DIENUM_CONTINUE;
        }

        DIPROPRANGE Range{};
        Range.diph.dwSize = sizeof(DIPROPRANGE);
        Range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        Range.diph.dwHow = DIPH_BYID;
        Range.diph.dwObj = Object->dwType;
        Range.lMin = DIRECT_INPUT_AXIS_MIN;
        Range.lMax = DIRECT_INPUT_AXIS_MAX;
        Device->SetProperty(DIPROP_RANGE, &Range.diph);
        return DIENUM_CONTINUE;
    }
}

struct FDirectInputRuntimeState
{
    IDirectInput8W* Interface = nullptr;
    IDirectInputDevice8W* Device = nullptr;
    GUID DeviceInstanceGuid = {};
    bool bLoggedDevice = false;
};

namespace
{
    BOOL CALLBACK EnumDirectInputDevicesCallback(const DIDEVICEINSTANCEW* Instance, void* Context)
    {
        FDirectInputRuntimeState* RuntimeState = static_cast<FDirectInputRuntimeState*>(Context);
        if (!RuntimeState || !RuntimeState->Interface || RuntimeState->Device)
        {
            return DIENUM_STOP;
        }

        IDirectInputDevice8W* Device = nullptr;
        const HRESULT Result = RuntimeState->Interface->CreateDevice(
            Instance->guidInstance,
            &Device,
            nullptr);

        if (FAILED(Result) || !Device)
        {
            return DIENUM_CONTINUE;
        }

        RuntimeState->Device = Device;
        RuntimeState->DeviceInstanceGuid = Instance->guidInstance;
        return DIENUM_STOP;
    }
}

bool FInputSystemSnapshot::IsDown(int InputCode) const
{
    if (InputCodes::IsKeyboardMouseCode(InputCode))
    {
        return KeyDown[InputCode];
    }

    const int ButtonIndex = InputCodes::GetGamepadButtonIndex(InputCode);
    if (ButtonIndex >= 0 && IsValidGamepadIndex(PrimaryGamepadIndex))
    {
        return Gamepads[PrimaryGamepadIndex].ButtonDown[ButtonIndex];
    }

    const int AxisIndex = InputCodes::GetGamepadAxisIndex(InputCode);
    if (AxisIndex >= 0)
    {
        return std::fabs(GetGamepadAxisValue(PrimaryGamepadIndex, InputCode)) > GAMEPAD_AXIS_DOWN_THRESHOLD;
    }

    return false;
}

bool FInputSystemSnapshot::WasPressed(int InputCode) const
{
    if (InputCodes::IsKeyboardMouseCode(InputCode))
    {
        return KeyPressed[InputCode];
    }

    const int ButtonIndex = InputCodes::GetGamepadButtonIndex(InputCode);
    if (ButtonIndex >= 0 && IsValidGamepadIndex(PrimaryGamepadIndex))
    {
        return Gamepads[PrimaryGamepadIndex].ButtonPressed[ButtonIndex];
    }

    return false;
}

bool FInputSystemSnapshot::WasReleased(int InputCode) const
{
    if (InputCodes::IsKeyboardMouseCode(InputCode))
    {
        return KeyReleased[InputCode];
    }

    const int ButtonIndex = InputCodes::GetGamepadButtonIndex(InputCode);
    if (ButtonIndex >= 0 && IsValidGamepadIndex(PrimaryGamepadIndex))
    {
        return Gamepads[PrimaryGamepadIndex].ButtonReleased[ButtonIndex];
    }

    return false;
}

float FInputSystemSnapshot::GetAxisValue(int InputCode) const
{
    if (InputCodes::IsKeyboardMouseCode(InputCode))
    {
        return KeyDown[InputCode] ? 1.0f : 0.0f;
    }

    const int ButtonIndex = InputCodes::GetGamepadButtonIndex(InputCode);
    if (ButtonIndex >= 0 && IsValidGamepadIndex(PrimaryGamepadIndex))
    {
        return Gamepads[PrimaryGamepadIndex].ButtonDown[ButtonIndex] ? 1.0f : 0.0f;
    }

    if (InputCodes::GetGamepadAxisIndex(InputCode) >= 0)
    {
        return GetGamepadAxisValue(PrimaryGamepadIndex, InputCode);
    }

    return 0.0f;
}

float FInputSystemSnapshot::GetGamepadAxisValue(int GamepadIndex, int AxisCode) const
{
    const int ResolvedGamepadIndex = ResolveGamepadIndex(GamepadIndex, PrimaryGamepadIndex);
    const int AxisIndex = InputCodes::GetGamepadAxisIndex(AxisCode);
    if (!IsValidGamepadIndex(ResolvedGamepadIndex) || AxisIndex < 0 || !Gamepads[ResolvedGamepadIndex].bConnected)
    {
        return 0.0f;
    }

    return Gamepads[ResolvedGamepadIndex].AxisValues[AxisIndex];
}

bool FInputSystemSnapshot::IsGamepadConnected(int GamepadIndex) const
{
    const int ResolvedGamepadIndex = GamepadIndex < 0 ? PrimaryGamepadIndex : GamepadIndex;
    return IsValidGamepadIndex(ResolvedGamepadIndex) && Gamepads[ResolvedGamepadIndex].bConnected;
}

bool InputSystem::GetKeyDown(int InputCode) const
{
    if (InputCodes::IsKeyboardMouseCode(InputCode))
    {
        return CurrentStates[InputCode] && !PrevStates[InputCode];
    }

    const int ButtonIndex = InputCodes::GetGamepadButtonIndex(InputCode);
    if (ButtonIndex >= 0 && IsValidGamepadIndex(PrimaryGamepadIndex))
    {
        return Gamepads[PrimaryGamepadIndex].ButtonDown[ButtonIndex]
            && !Gamepads[PrimaryGamepadIndex].PrevButtonDown[ButtonIndex];
    }

    return false;
}

bool InputSystem::GetKey(int InputCode) const
{
    if (InputCodes::IsKeyboardMouseCode(InputCode))
    {
        return CurrentStates[InputCode];
    }

    const int ButtonIndex = InputCodes::GetGamepadButtonIndex(InputCode);
    if (ButtonIndex >= 0 && IsValidGamepadIndex(PrimaryGamepadIndex))
    {
        return Gamepads[PrimaryGamepadIndex].ButtonDown[ButtonIndex];
    }

    const int AxisIndex = InputCodes::GetGamepadAxisIndex(InputCode);
    if (AxisIndex >= 0)
    {
        return std::fabs(GetGamepadAxisValue(PrimaryGamepadIndex, InputCode)) > GAMEPAD_AXIS_DOWN_THRESHOLD;
    }

    return false;
}

bool InputSystem::GetKeyUp(int InputCode) const
{
    if (InputCodes::IsKeyboardMouseCode(InputCode))
    {
        return !CurrentStates[InputCode] && PrevStates[InputCode];
    }

    const int ButtonIndex = InputCodes::GetGamepadButtonIndex(InputCode);
    if (ButtonIndex >= 0 && IsValidGamepadIndex(PrimaryGamepadIndex))
    {
        return !Gamepads[PrimaryGamepadIndex].ButtonDown[ButtonIndex]
            && Gamepads[PrimaryGamepadIndex].PrevButtonDown[ButtonIndex];
    }

    return false;
}

float InputSystem::GetAxisValue(int InputCode) const
{
    if (InputCodes::IsKeyboardMouseCode(InputCode))
    {
        return CurrentStates[InputCode] ? 1.0f : 0.0f;
    }

    const int ButtonIndex = InputCodes::GetGamepadButtonIndex(InputCode);
    if (ButtonIndex >= 0 && IsValidGamepadIndex(PrimaryGamepadIndex))
    {
        return Gamepads[PrimaryGamepadIndex].ButtonDown[ButtonIndex] ? 1.0f : 0.0f;
    }

    if (InputCodes::GetGamepadAxisIndex(InputCode) >= 0)
    {
        return GetGamepadAxisValue(PrimaryGamepadIndex, InputCode);
    }

    return 0.0f;
}

float InputSystem::GetGamepadAxisValue(int GamepadIndex, int AxisCode) const
{
    const int ResolvedGamepadIndex = ResolveGamepadIndex(GamepadIndex, PrimaryGamepadIndex);
    const int AxisIndex = InputCodes::GetGamepadAxisIndex(AxisCode);
    if (!IsValidGamepadIndex(ResolvedGamepadIndex) || AxisIndex < 0 || !Gamepads[ResolvedGamepadIndex].bConnected)
    {
        return 0.0f;
    }

    return Gamepads[ResolvedGamepadIndex].AxisValues[AxisIndex];
}

bool InputSystem::IsGamepadConnected(int GamepadIndex) const
{
    const int ResolvedGamepadIndex = GamepadIndex < 0 ? PrimaryGamepadIndex : GamepadIndex;
    return IsValidGamepadIndex(ResolvedGamepadIndex) && Gamepads[ResolvedGamepadIndex].bConnected;
}

InputSystem::~InputSystem()
{
    ReleaseDirectInputGamepad();
    if (DirectInputState)
    {
        if (DirectInputState->Interface)
        {
            DirectInputState->Interface->Release();
            DirectInputState->Interface = nullptr;
        }

        delete DirectInputState;
        DirectInputState = nullptr;
    }
}

bool InputSystem::EnsureDirectInputGamepad()
{
    if (DirectInputState && DirectInputState->Device)
    {
        return true;
    }

    if (DirectInputRetryFramesRemaining > 0)
    {
        --DirectInputRetryFramesRemaining;
        return false;
    }

    if (!DirectInputState)
    {
        DirectInputState = new FDirectInputRuntimeState();
    }

    if (!DirectInputState->Interface)
    {
        const HRESULT Result = DirectInput8Create(
            GetModuleHandleW(nullptr),
            DIRECTINPUT_VERSION,
            IID_IDirectInput8W,
            reinterpret_cast<void**>(&DirectInputState->Interface),
            nullptr);

        if (FAILED(Result) || !DirectInputState->Interface)
        {
            DirectInputRetryFramesRemaining = DIRECT_INPUT_RETRY_FRAME_COUNT;
            return false;
        }
    }

    DirectInputState->Interface->EnumDevices(
        DI8DEVCLASS_GAMECTRL,
        EnumDirectInputDevicesCallback,
        DirectInputState,
        DIEDFL_ATTACHEDONLY);

    if (!DirectInputState->Device)
    {
        DirectInputRetryFramesRemaining = DIRECT_INPUT_RETRY_FRAME_COUNT;
        return false;
    }

    HRESULT Result = DirectInputState->Device->SetDataFormat(&c_dfDIJoystick2);
    if (FAILED(Result))
    {
        ReleaseDirectInputGamepad();
        DirectInputRetryFramesRemaining = DIRECT_INPUT_RETRY_FRAME_COUNT;
        return false;
    }

    if (OwnerHWnd)
    {
        Result = DirectInputState->Device->SetCooperativeLevel(
            OwnerHWnd,
            DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);

        if (FAILED(Result))
        {
            UE_LOG("[Input] DirectInput SetCooperativeLevel failed: 0x%08X", static_cast<unsigned>(Result));
        }
    }

    DirectInputState->Device->EnumObjects(EnumDirectInputAxesCallback, DirectInputState->Device, DIDFT_AXIS);
    DirectInputState->Device->Acquire();

    if (!DirectInputState->bLoggedDevice)
    {
        UE_LOG("[Input] DirectInput gamepad fallback active.");
        DirectInputState->bLoggedDevice = true;
    }

    return true;
}

void InputSystem::ReleaseDirectInputGamepad()
{
    if (!DirectInputState || !DirectInputState->Device)
    {
        return;
    }

    DirectInputState->Device->Unacquire();
    DirectInputState->Device->Release();
    DirectInputState->Device = nullptr;
    DirectInputState->bLoggedDevice = false;
}

void InputSystem::Tick()
{
    // 윈도우 포커스가 없으면 모든 입력 상태 해제
    bWindowFocused = !OwnerHWnd || GetForegroundWindow() == OwnerHWnd;
    if (!bWindowFocused)
    {
        ResetAllKeyStates();
        ResetTransientState();
        UpdateCurrentSnapshot();
        return;
    }

    for (int i = 0; i < 256; ++i)
    {
        PrevStates[i] = CurrentStates[i];
        CurrentStates[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
    }

    PollGamepads();

    bLeftDragJustStarted = false;
    bRightDragJustStarted = false;
    bLeftDragJustEnded = false;
    bRightDragJustEnded = false;

    PrevScrollDelta = ScrollDelta;
    ScrollDelta = 0;

    PrevMousePos = MousePos;
    GetCursorPos(&MousePos);
    FrameMouseDeltaX = MousePos.x - PrevMousePos.x;
    FrameMouseDeltaY = MousePos.y - PrevMousePos.y;
    if (bUseRawMouse)
    {
        FrameMouseDeltaX = RawMouseDeltaAccumX;
        FrameMouseDeltaY = RawMouseDeltaAccumY;
    }
    RawMouseDeltaAccumX = 0;
    RawMouseDeltaAccumY = 0;

    if (GetKeyDown(VK_LBUTTON))
    {
        bLeftDragCandidate = true;
        LeftMouseDownPos = MousePos;
    }
    if (GetKeyDown(VK_RBUTTON))
    {
        bRightDragCandidate = true;
        RightMouseDownPos = MousePos;
    }

    // Left drag
    if (!bLeftDragging && IsDraggingLeft())
    {
        FilterDragThreshold(bLeftDragCandidate, bLeftDragging, bLeftDragJustStarted,
            LeftMouseDownPos, LeftDragStartPos);
    }
    else if (GetKeyUp(VK_LBUTTON))
    {
        if (bLeftDragging) bLeftDragJustEnded = true;
        bLeftDragging = false;
        bLeftDragCandidate = false;
    }

    // Right drag
    if (!bRightDragging && IsDraggingRight())
    {
        FilterDragThreshold(bRightDragCandidate, bRightDragging, bRightDragJustStarted,
            RightMouseDownPos, RightDragStartPos);
    }
    else if (GetKeyUp(VK_RBUTTON))
    {
        if (bRightDragging) bRightDragJustEnded = true;
        bRightDragging = false;
        bRightDragCandidate = false;
    }

    UpdateCurrentSnapshot();
}

FInputSystemSnapshot InputSystem::TickAndMakeSnapshot()
{
    Tick();
    return MakeSnapshot();
}

FInputSystemSnapshot InputSystem::MakeSnapshot() const
{
    return CurrentSnapshot;
}

void InputSystem::RefreshSnapshot()
{
    UpdateCurrentSnapshot();
}

void InputSystem::SetUseRawMouse(bool bEnable)
{
    if (bUseRawMouse == bEnable)
    {
        return;
    }

    bUseRawMouse = bEnable;
    ResetMouseDelta();
    UpdateCurrentSnapshot();
}

void InputSystem::AddRawMouseDelta(int DeltaX, int DeltaY)
{
    RawMouseDeltaAccumX += DeltaX;
    RawMouseDeltaAccumY += DeltaY;
}

void InputSystem::ResetTransientState()
{
    bLeftDragJustStarted = false;
    bRightDragJustStarted = false;
    bLeftDragJustEnded = false;
    bRightDragJustEnded = false;
    ResetDragState();
    ResetMouseDelta();
    ResetWheelDelta();
    UpdateCurrentSnapshot();
}

void InputSystem::ResetAllKeyStates()
{
    for (int VK = 0; VK < 256; ++VK)
    {
        CurrentStates[VK] = false;
        PrevStates[VK] = false;
    }
    ResetGamepadInputState();
    UpdateCurrentSnapshot();
}

void InputSystem::ResetMouseDelta()
{
    GetCursorPos(&MousePos);
    PrevMousePos = MousePos;
    FrameMouseDeltaX = 0;
    FrameMouseDeltaY = 0;
    RawMouseDeltaAccumX = 0;
    RawMouseDeltaAccumY = 0;
    UpdateCurrentSnapshot();
}

void InputSystem::ResetWheelDelta()
{
    ScrollDelta = 0;
    PrevScrollDelta = 0;
    UpdateCurrentSnapshot();
}

void InputSystem::ResetCaptureStateForPIEEnd()
{
    SetUseRawMouse(false);
    ResetAllKeyStates();
    ResetTransientState();
    GuiState.bUsingMouse = false;
    GuiState.bUsingKeyboard = false;
    GuiState.bUsingTextInput = false;
    UpdateCurrentSnapshot();
}

void InputSystem::PollDirectInputGamepad(int& FirstConnectedGamepad)
{
    if (FirstConnectedGamepad >= 0 || !EnsureDirectInputGamepad())
    {
        return;
    }

    DIJOYSTATE2 State{};
    HRESULT Result = DirectInputState->Device->Poll();
    if (FAILED(Result))
    {
        DirectInputState->Device->Acquire();
    }

    Result = DirectInputState->Device->GetDeviceState(sizeof(DIJOYSTATE2), &State);
    if (FAILED(Result))
    {
        DirectInputState->Device->Acquire();
        Result = DirectInputState->Device->GetDeviceState(sizeof(DIJOYSTATE2), &State);
    }

    if (FAILED(Result))
    {
        ReleaseDirectInputGamepad();
        DirectInputRetryFramesRemaining = DIRECT_INPUT_RETRY_FRAME_COUNT;
        return;
    }

    FGamepadRuntimeState& Gamepad = Gamepads[0];
    Gamepad.bConnected = true;
    FirstConnectedGamepad = 0;

    for (int ButtonIndex = 0; ButtonIndex < InputCodes::GamepadButtonCount; ++ButtonIndex)
    {
        Gamepad.ButtonDown[ButtonIndex] = false;
    }
    for (int AxisIndex = 0; AxisIndex < InputCodes::GamepadAxisCount; ++AxisIndex)
    {
        Gamepad.AxisValues[AxisIndex] = 0.0f;
    }

    const auto SetButton = [&Gamepad](int InputCode, bool bDown)
    {
        const int ButtonIndex = InputCodes::GetGamepadButtonIndex(InputCode);
        if (ButtonIndex >= 0)
        {
            Gamepad.ButtonDown[ButtonIndex] = bDown;
        }
    };

    const auto SetAxis = [&Gamepad](int InputCode, float Value)
    {
        const int AxisIndex = InputCodes::GetGamepadAxisIndex(InputCode);
        if (AxisIndex >= 0)
        {
            Gamepad.AxisValues[AxisIndex] = Value;
        }
    };

    SetButton(InputCodes::GamepadX, IsDirectInputButtonDown(State, 0));
    SetButton(InputCodes::GamepadA, IsDirectInputButtonDown(State, 1));
    SetButton(InputCodes::GamepadB, IsDirectInputButtonDown(State, 2));
    SetButton(InputCodes::GamepadY, IsDirectInputButtonDown(State, 3));
    SetButton(InputCodes::GamepadLeftShoulder, IsDirectInputButtonDown(State, 4));
    SetButton(InputCodes::GamepadRightShoulder, IsDirectInputButtonDown(State, 5));
    SetButton(InputCodes::GamepadLeftTrigger, IsDirectInputButtonDown(State, 6));
    SetButton(InputCodes::GamepadRightTrigger, IsDirectInputButtonDown(State, 7));
    SetButton(InputCodes::GamepadBack, IsDirectInputButtonDown(State, 8));
    SetButton(InputCodes::GamepadStart, IsDirectInputButtonDown(State, 9));
    SetButton(InputCodes::GamepadLeftThumb, IsDirectInputButtonDown(State, 10));
    SetButton(InputCodes::GamepadRightThumb, IsDirectInputButtonDown(State, 11));

    const DWORD Pov = State.rgdwPOV[0];
    if (Pov != 0xFFFFFFFFu)
    {
        SetButton(InputCodes::GamepadDPadUp, Pov >= 31500 || Pov <= 4500);
        SetButton(InputCodes::GamepadDPadRight, Pov >= 4500 && Pov <= 13500);
        SetButton(InputCodes::GamepadDPadDown, Pov >= 13500 && Pov <= 22500);
        SetButton(InputCodes::GamepadDPadLeft, Pov >= 22500 && Pov <= 31500);
    }

    SetAxis(InputCodes::GamepadLeftX, NormalizeDirectInputAxis(State.lX, false));
    SetAxis(InputCodes::GamepadLeftY, NormalizeDirectInputAxis(State.lY, true));
    SetAxis(
        InputCodes::GamepadRightX,
        PickLargerMagnitude(
            NormalizeDirectInputAxis(State.lZ, false),
            NormalizeDirectInputAxis(State.lRx, false)));
    SetAxis(
        InputCodes::GamepadRightY,
        PickLargerMagnitude(
            NormalizeDirectInputAxis(State.lRz, true),
            NormalizeDirectInputAxis(State.lRy, true)));
    SetAxis(InputCodes::GamepadLeftTriggerAxis, IsDirectInputButtonDown(State, 6) ? 1.0f : 0.0f);
    SetAxis(InputCodes::GamepadRightTriggerAxis, IsDirectInputButtonDown(State, 7) ? 1.0f : 0.0f);
}

void InputSystem::UpdateCurrentSnapshot()
{
    FInputSystemSnapshot Snapshot{};
    for (int VK = 0; VK < 256; ++VK)
    {
        Snapshot.KeyDown[VK] = CurrentStates[VK];
        Snapshot.KeyPressed[VK] = CurrentStates[VK] && !PrevStates[VK];
        Snapshot.KeyReleased[VK] = !CurrentStates[VK] && PrevStates[VK];
    }

    Snapshot.PrimaryGamepadIndex = PrimaryGamepadIndex;
    for (int GamepadIndex = 0; GamepadIndex < InputCodes::MaxGamepads; ++GamepadIndex)
    {
        const FGamepadRuntimeState& Source = Gamepads[GamepadIndex];
        FGamepadInputSnapshot& Target = Snapshot.Gamepads[GamepadIndex];

        Target.bConnected = Source.bConnected;
        for (int ButtonIndex = 0; ButtonIndex < InputCodes::GamepadButtonCount; ++ButtonIndex)
        {
            Target.ButtonDown[ButtonIndex] = Source.ButtonDown[ButtonIndex];
            Target.ButtonPressed[ButtonIndex] = Source.ButtonDown[ButtonIndex] && !Source.PrevButtonDown[ButtonIndex];
            Target.ButtonReleased[ButtonIndex] = !Source.ButtonDown[ButtonIndex] && Source.PrevButtonDown[ButtonIndex];
        }
        for (int AxisIndex = 0; AxisIndex < InputCodes::GamepadAxisCount; ++AxisIndex)
        {
            Target.AxisValues[AxisIndex] = Source.AxisValues[AxisIndex];
        }
    }

    Snapshot.bLeftMouseDown = Snapshot.KeyDown[VK_LBUTTON];
    Snapshot.bLeftMousePressed = Snapshot.KeyPressed[VK_LBUTTON];
    Snapshot.bLeftMouseReleased = Snapshot.KeyReleased[VK_LBUTTON];
    Snapshot.bRightMouseDown = Snapshot.KeyDown[VK_RBUTTON];
    Snapshot.bRightMousePressed = Snapshot.KeyPressed[VK_RBUTTON];
    Snapshot.bRightMouseReleased = Snapshot.KeyReleased[VK_RBUTTON];
    Snapshot.bMiddleMouseDown = Snapshot.KeyDown[VK_MBUTTON];
    Snapshot.bMiddleMousePressed = Snapshot.KeyPressed[VK_MBUTTON];
    Snapshot.bMiddleMouseReleased = Snapshot.KeyReleased[VK_MBUTTON];
    Snapshot.bXButton1Down = Snapshot.KeyDown[VK_XBUTTON1];
    Snapshot.bXButton1Pressed = Snapshot.KeyPressed[VK_XBUTTON1];
    Snapshot.bXButton1Released = Snapshot.KeyReleased[VK_XBUTTON1];
    Snapshot.bXButton2Down = Snapshot.KeyDown[VK_XBUTTON2];
    Snapshot.bXButton2Pressed = Snapshot.KeyPressed[VK_XBUTTON2];
    Snapshot.bXButton2Released = Snapshot.KeyReleased[VK_XBUTTON2];

    Snapshot.MousePos = MousePos;
    Snapshot.MouseDeltaX = FrameMouseDeltaX;
    Snapshot.MouseDeltaY = FrameMouseDeltaY;
    Snapshot.ScrollDelta = PrevScrollDelta;

    Snapshot.bLeftDragStarted = bLeftDragJustStarted;
    Snapshot.bLeftDragging = bLeftDragging;
    Snapshot.bLeftDragEnded = bLeftDragJustEnded;
    Snapshot.LeftDragVector = GetLeftDragVector();

    Snapshot.bRightDragStarted = bRightDragJustStarted;
    Snapshot.bRightDragging = bRightDragging;
    Snapshot.bRightDragEnded = bRightDragJustEnded;
    Snapshot.RightDragVector = GetRightDragVector();

    Snapshot.bUsingRawMouse = bUseRawMouse;
    Snapshot.bGuiUsingMouse = GuiState.bUsingMouse;
    Snapshot.bGuiUsingKeyboard = GuiState.bUsingKeyboard;
    Snapshot.bGuiUsingTextInput = GuiState.bUsingTextInput;
    Snapshot.bWindowFocused = bWindowFocused;
    CurrentSnapshot = Snapshot;
}

void InputSystem::PollGamepads()
{
    int FirstConnectedGamepad = -1;

    for (int GamepadIndex = 0; GamepadIndex < InputCodes::MaxGamepads; ++GamepadIndex)
    {
        FGamepadRuntimeState& Gamepad = Gamepads[GamepadIndex];

        // 직전 프레임 버튼 상태 보관 — XInput 연결 해제 시에도 release edge 계산 가능
        for (int ButtonIndex = 0; ButtonIndex < InputCodes::GamepadButtonCount; ++ButtonIndex)
        {
            Gamepad.PrevButtonDown[ButtonIndex] = Gamepad.ButtonDown[ButtonIndex];
        }

        XINPUT_STATE State{};
        const DWORD Result = XInputGetState(static_cast<DWORD>(GamepadIndex), &State);
        if (Result != ERROR_SUCCESS)
        {
            Gamepad.bConnected = false;
            for (int ButtonIndex = 0; ButtonIndex < InputCodes::GamepadButtonCount; ++ButtonIndex)
            {
                Gamepad.ButtonDown[ButtonIndex] = false;
            }
            for (int AxisIndex = 0; AxisIndex < InputCodes::GamepadAxisCount; ++AxisIndex)
            {
                Gamepad.AxisValues[AxisIndex] = 0.0f;
            }
            continue;
        }

        Gamepad.bConnected = true;
        if (FirstConnectedGamepad < 0)
        {
            FirstConnectedGamepad = GamepadIndex;
        }

        const XINPUT_GAMEPAD& XGamepad = State.Gamepad;

        // 디지털 버튼 매핑 — trigger는 아래에서 threshold 기반 버튼으로 별도 처리
        for (int ButtonIndex = 0; ButtonIndex < InputCodes::GamepadButtonCount; ++ButtonIndex)
        {
            const WORD Mask = GGamepadButtonMasks[ButtonIndex];
            Gamepad.ButtonDown[ButtonIndex] = Mask != 0 && ((XGamepad.wButtons & Mask) != 0);
        }

        // trigger는 FPS 사격/조준용 action에 바로 묶을 수 있도록 버튼 edge도 제공
        Gamepad.ButtonDown[InputCodes::GamepadLeftTrigger - InputCodes::GamepadButtonBase] =
            XGamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
        Gamepad.ButtonDown[InputCodes::GamepadRightTrigger - InputCodes::GamepadButtonBase] =
            XGamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;

        // 아날로그 축 매핑 — stick은 -1~1, trigger는 0~1
        Gamepad.AxisValues[InputCodes::GamepadLeftX - InputCodes::GamepadAxisBase] =
            NormalizeStickAxis(XGamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        Gamepad.AxisValues[InputCodes::GamepadLeftY - InputCodes::GamepadAxisBase] =
            NormalizeStickAxis(XGamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        Gamepad.AxisValues[InputCodes::GamepadRightX - InputCodes::GamepadAxisBase] =
            NormalizeStickAxis(XGamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
        Gamepad.AxisValues[InputCodes::GamepadRightY - InputCodes::GamepadAxisBase] =
            NormalizeStickAxis(XGamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
        Gamepad.AxisValues[InputCodes::GamepadLeftTriggerAxis - InputCodes::GamepadAxisBase] =
            NormalizeTriggerAxis(XGamepad.bLeftTrigger);
        Gamepad.AxisValues[InputCodes::GamepadRightTriggerAxis - InputCodes::GamepadAxisBase] =
            NormalizeTriggerAxis(XGamepad.bRightTrigger);
    }

    PollDirectInputGamepad(FirstConnectedGamepad);
    PrimaryGamepadIndex = FirstConnectedGamepad >= 0 ? FirstConnectedGamepad : 0;
}

void InputSystem::ResetGamepadInputState()
{
    PrimaryGamepadIndex = 0;
    for (FGamepadRuntimeState& Gamepad : Gamepads)
    {
        Gamepad.bConnected = false;
        for (int ButtonIndex = 0; ButtonIndex < InputCodes::GamepadButtonCount; ++ButtonIndex)
        {
            Gamepad.ButtonDown[ButtonIndex] = false;
            Gamepad.PrevButtonDown[ButtonIndex] = false;
        }
        for (int AxisIndex = 0; AxisIndex < InputCodes::GamepadAxisCount; ++AxisIndex)
        {
            Gamepad.AxisValues[AxisIndex] = 0.0f;
        }
    }
}

void InputSystem::ResetDragState()
{
    bLeftDragCandidate = false;
    bRightDragCandidate = false;
    bLeftDragging = false;
    bRightDragging = false;
    bLeftDragJustStarted = false;
    bRightDragJustStarted = false;
    bLeftDragJustEnded = false;
    bRightDragJustEnded = false;
    LeftDragStartPos = MousePos;
    LeftMouseDownPos = MousePos;
    RightDragStartPos = MousePos;
    RightMouseDownPos = MousePos;
}

void InputSystem::FilterDragThreshold(
    bool& bCandidate, bool& bDragging, bool& bJustStarted,
    const POINT& MouseDownPos, POINT& DragStartPos)
{
    if (bCandidate && !bDragging)
    {
        int DX = MousePos.x - MouseDownPos.x;
        int DY = MousePos.y - MouseDownPos.y;
        int DistSq = DX * DX + DY * DY;

        if (DistSq >= DRAG_THRESHOLD * DRAG_THRESHOLD)
        {
            bJustStarted = true;
            bDragging = true;
            DragStartPos = MouseDownPos;
        }
    }
}

POINT InputSystem::GetLeftDragVector() const
{
    POINT V;
    V.x = MousePos.x - LeftDragStartPos.x;
    V.y = MousePos.y - LeftDragStartPos.y;
    return V;
}

POINT InputSystem::GetRightDragVector() const
{
    POINT V;
    V.x = MousePos.x - RightDragStartPos.x;
    V.y = MousePos.y - RightDragStartPos.y;
    return V;
}

float InputSystem::GetLeftDragDistance() const
{
    POINT V = GetLeftDragVector();
    return std::sqrt((float)(V.x * V.x + V.y * V.y));
}

float InputSystem::GetRightDragDistance() const
{
    POINT V = GetRightDragVector();
    return std::sqrt((float)(V.x * V.x + V.y * V.y));
}
