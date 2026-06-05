#pragma once
#include <windows.h>
#include "Core/Singleton.h"
#include "Core/Types/CoreTypes.h"

/**
* @brief 엔진 전역 입력 코드 상수 모음
*
* @details 0~255 영역은 기존 Win32 VK 코드를 그대로 사용하고, 256 이상 영역은
* 게임패드 버튼과 아날로그 축을 위한 엔진 가상 코드로 사용합니다.
*/
namespace InputCodes
{
    constexpr int KeyboardMouseCodeCount = 256;

    constexpr int MaxGamepads = 4;

    constexpr int GamepadButtonBase = 0x100;
    constexpr int GamepadButtonCount = 16;

    constexpr int GamepadA = GamepadButtonBase + 0;
    constexpr int GamepadB = GamepadButtonBase + 1;
    constexpr int GamepadX = GamepadButtonBase + 2;
    constexpr int GamepadY = GamepadButtonBase + 3;
    constexpr int GamepadLeftShoulder = GamepadButtonBase + 4;
    constexpr int GamepadRightShoulder = GamepadButtonBase + 5;
    constexpr int GamepadBack = GamepadButtonBase + 6;
    constexpr int GamepadStart = GamepadButtonBase + 7;
    constexpr int GamepadLeftThumb = GamepadButtonBase + 8;
    constexpr int GamepadRightThumb = GamepadButtonBase + 9;
    constexpr int GamepadDPadUp = GamepadButtonBase + 10;
    constexpr int GamepadDPadDown = GamepadButtonBase + 11;
    constexpr int GamepadDPadLeft = GamepadButtonBase + 12;
    constexpr int GamepadDPadRight = GamepadButtonBase + 13;
    constexpr int GamepadLeftTrigger = GamepadButtonBase + 14;
    constexpr int GamepadRightTrigger = GamepadButtonBase + 15;

    constexpr int GamepadAxisBase = 0x200;
    constexpr int GamepadAxisCount = 6;

    constexpr int GamepadLeftX = GamepadAxisBase + 0;
    constexpr int GamepadLeftY = GamepadAxisBase + 1;
    constexpr int GamepadRightX = GamepadAxisBase + 2;
    constexpr int GamepadRightY = GamepadAxisBase + 3;
    constexpr int GamepadLeftTriggerAxis = GamepadAxisBase + 4;
    constexpr int GamepadRightTriggerAxis = GamepadAxisBase + 5;

    /**
    * @brief Win32 키보드/마우스 코드 여부를 반환합니다.
    *
    * @param InputCode 판정할 입력 코드
    *
    * @return 0~255 VK 영역에 포함되면 true
    */
    constexpr bool IsKeyboardMouseCode(int InputCode)
    {
        return InputCode >= 0 && InputCode < KeyboardMouseCodeCount;
    }

    /**
    * @brief 게임패드 버튼 코드에서 버튼 인덱스를 계산합니다.
    *
    * @param InputCode 판정할 입력 코드
    *
    * @return 버튼 인덱스. 유효하지 않으면 -1
    */
    constexpr int GetGamepadButtonIndex(int InputCode)
    {
        return (InputCode >= GamepadButtonBase && InputCode < GamepadButtonBase + GamepadButtonCount)
            ? InputCode - GamepadButtonBase
            : -1;
    }

    /**
    * @brief 게임패드 축 코드에서 축 인덱스를 계산합니다.
    *
    * @param InputCode 판정할 입력 코드
    *
    * @return 축 인덱스. 유효하지 않으면 -1
    */
    constexpr int GetGamepadAxisIndex(int InputCode)
    {
        return (InputCode >= GamepadAxisBase && InputCode < GamepadAxisBase + GamepadAxisCount)
            ? InputCode - GamepadAxisBase
            : -1;
    }
}

struct FGuiInputState
{
    bool bUsingMouse = false;
    bool bUsingKeyboard = false;
    bool bUsingTextInput = false;
};

/**
* @brief 단일 게임패드의 한 프레임 입력 스냅샷
*/
struct FGamepadInputSnapshot
{
    bool bConnected = false;
    bool ButtonDown[InputCodes::GamepadButtonCount] = {};
    bool ButtonPressed[InputCodes::GamepadButtonCount] = {};
    bool ButtonReleased[InputCodes::GamepadButtonCount] = {};
    float AxisValues[InputCodes::GamepadAxisCount] = {};
};

/**
* @brief 입력 시스템의 한 프레임 불변 스냅샷
*/
struct FInputSystemSnapshot
{
    bool KeyDown[256] = {};
    bool KeyPressed[256] = {};
    bool KeyReleased[256] = {};

    POINT MousePos = { 0, 0 };
    int MouseDeltaX = 0;
    int MouseDeltaY = 0;
    int ScrollDelta = 0;

    bool bLeftMouseDown = false;
    bool bLeftMousePressed = false;
    bool bLeftMouseReleased = false;
    bool bRightMouseDown = false;
    bool bRightMousePressed = false;
    bool bRightMouseReleased = false;
    bool bMiddleMouseDown = false;
    bool bMiddleMousePressed = false;
    bool bMiddleMouseReleased = false;
    bool bXButton1Down = false;
    bool bXButton1Pressed = false;
    bool bXButton1Released = false;
    bool bXButton2Down = false;
    bool bXButton2Pressed = false;
    bool bXButton2Released = false;

    bool bLeftDragStarted = false;
    bool bLeftDragging = false;
    bool bLeftDragEnded = false;
    POINT LeftDragVector = { 0, 0 };

    bool bRightDragStarted = false;
    bool bRightDragging = false;
    bool bRightDragEnded = false;
    POINT RightDragVector = { 0, 0 };

    bool bUsingRawMouse = false;
    bool bGuiUsingMouse = false;
    bool bGuiUsingKeyboard = false;
    bool bGuiUsingTextInput = false;
    bool bWindowFocused = true;

    FGamepadInputSnapshot Gamepads[InputCodes::MaxGamepads] = {};
    int PrimaryGamepadIndex = 0;

    /**
    * @brief 입력 코드가 현재 눌려 있는지 반환합니다.
    *
    * @param InputCode Win32 VK 코드 또는 InputCodes 게임패드 가상 코드
    *
    * @return 현재 눌림 상태
    */
    bool IsDown(int InputCode) const;

    /**
    * @brief 입력 코드가 이번 프레임에 눌렸는지 반환합니다.
    *
    * @param InputCode Win32 VK 코드 또는 InputCodes 게임패드 버튼 가상 코드
    *
    * @return 이번 프레임 눌림 edge 상태
    */
    bool WasPressed(int InputCode) const;

    /**
    * @brief 입력 코드가 이번 프레임에 해제되었는지 반환합니다.
    *
    * @param InputCode Win32 VK 코드 또는 InputCodes 게임패드 버튼 가상 코드
    *
    * @return 이번 프레임 해제 edge 상태
    */
    bool WasReleased(int InputCode) const;

    /**
    * @brief 입력 코드를 축 값으로 평가합니다.
    *
    * @param InputCode Win32 VK 코드, 게임패드 버튼 코드 또는 게임패드 축 코드
    *
    * @return 키/버튼은 눌림 시 1.0, 해제 시 0.0. 게임패드 축은 정규화된 축 값
    */
    float GetAxisValue(int InputCode) const;

    /**
    * @brief 지정 게임패드의 축 값을 반환합니다.
    *
    * @param GamepadIndex XInput 게임패드 인덱스. -1이면 primary 게임패드 인덱스
    *
    * @param AxisCode InputCodes 게임패드 축 코드
    *
    * @return 정규화된 축 값. 유효하지 않으면 0.0
    */
    float GetGamepadAxisValue(int GamepadIndex, int AxisCode) const;

    /**
    * @brief 지정 게임패드의 연결 여부를 반환합니다.
    *
    * @param GamepadIndex XInput 게임패드 인덱스
    *
    * @return 연결되어 있으면 true
    */
    bool IsGamepadConnected(int GamepadIndex = -1) const;
};

class InputSystem : public TSingleton<InputSystem>
{
	friend class TSingleton<InputSystem>;

public:
    void Tick();
    FInputSystemSnapshot TickAndMakeSnapshot();
    FInputSystemSnapshot MakeSnapshot() const;
    void RefreshSnapshot();
    void SetUseRawMouse(bool bEnable);
    bool IsUsingRawMouse() const { return bUseRawMouse; }
    void AddRawMouseDelta(int DeltaX, int DeltaY);
    void ResetTransientState();
    void ResetAllKeyStates();
    void ResetMouseDelta();
    void ResetWheelDelta();
    void ResetCaptureStateForPIEEnd();
    bool IsWindowFocused() const { return bWindowFocused; }

    // Keyboard
    bool GetKeyDown(int InputCode) const;
    bool GetKey(int InputCode) const;
    bool GetKeyUp(int InputCode) const;
    float GetAxisValue(int InputCode) const;
    float GetGamepadAxisValue(int GamepadIndex, int AxisCode) const;
    bool IsGamepadConnected(int GamepadIndex = -1) const;

    // Mouse position
    POINT GetMousePos() const { return MousePos; }
    POINT GetMouseClientPos() const
    {
        POINT ClientPos = MousePos;
        if (OwnerHWnd)
        {
            ScreenToClient(OwnerHWnd, &ClientPos);
        }
        return ClientPos;
    }
    int MouseDeltaX() const { return FrameMouseDeltaX; }
    int MouseDeltaY() const { return FrameMouseDeltaY; }
    bool MouseMoved() const { return MouseDeltaX() != 0 || MouseDeltaY() != 0; }

    // Left drag
    bool IsDraggingLeft() const { return GetKey(VK_LBUTTON) && MouseMoved(); }
    bool GetLeftDragStart() const { return bLeftDragJustStarted; }
    bool GetLeftDragging() const { return bLeftDragging; }
    bool GetLeftDragEnd() const { return bLeftDragJustEnded; }
    POINT GetLeftDragVector() const;
    float GetLeftDragDistance() const;

    // Right drag
    bool IsDraggingRight() const { return GetKey(VK_RBUTTON) && MouseMoved(); }
    bool GetRightDragStart() const { return bRightDragJustStarted; }
    bool GetRightDragging() const { return bRightDragging; }
    bool GetRightDragEnd() const { return bRightDragJustEnded; }
    POINT GetRightDragVector() const;
    float GetRightDragDistance() const;

    // Scrolling
    void AddScrollDelta(int Delta) { ScrollDelta += Delta; }
    int GetScrollDelta() const { return PrevScrollDelta; }
    bool ScrolledUp() const { return PrevScrollDelta > 0; }
    bool ScrolledDown() const { return PrevScrollDelta < 0; }
    float GetScrollNotches() const { return PrevScrollDelta / (float)WHEEL_DELTA; }

    // Window focus
    void SetOwnerWindow(HWND InHWnd) { OwnerHWnd = InHWnd; }

    // GUI state
    FGuiInputState& GetGuiInputState() { return GuiState; }
    const FGuiInputState& GetGuiInputState() const { return GuiState; }
    void SetGuiMouseCapture(bool bCapture) { GuiState.bUsingMouse = bCapture; }
    void SetGuiKeyboardCapture(bool bCapture) { GuiState.bUsingKeyboard = bCapture; }
    void SetGuiTextInputCapture(bool bCapture) { GuiState.bUsingTextInput = bCapture; }
    bool IsGuiUsingMouse() const { return GuiState.bUsingMouse; }
    bool IsGuiUsingKeyboard() const { return GuiState.bUsingKeyboard; }
    bool IsGuiUsingTextInput() const { return GuiState.bUsingTextInput; }

private:
    struct FGamepadRuntimeState
    {
        bool bConnected = false;
        bool ButtonDown[InputCodes::GamepadButtonCount] = {};
        bool PrevButtonDown[InputCodes::GamepadButtonCount] = {};
        float AxisValues[InputCodes::GamepadAxisCount] = {};
    };

    bool CurrentStates[256] = { false };
    bool PrevStates[256] = { false };
    FGamepadRuntimeState Gamepads[InputCodes::MaxGamepads] = {};
    int PrimaryGamepadIndex = 0;

    // Mouse members
    POINT MousePos = { 0, 0 };
    POINT PrevMousePos = { 0, 0 };
    int FrameMouseDeltaX = 0;
    int FrameMouseDeltaY = 0;
    int RawMouseDeltaAccumX = 0;
    int RawMouseDeltaAccumY = 0;
    bool bUseRawMouse = false;

    bool bLeftDragCandidate = false;
    bool bRightDragCandidate = false;
    bool bLeftDragging = false;
    bool bRightDragging = false;

    bool bLeftDragJustStarted = false;
    bool bRightDragJustStarted = false;
    bool bLeftDragJustEnded = false;
    bool bRightDragJustEnded = false;

    // Drag origin
    POINT LeftDragStartPos = { 0, 0 };
    POINT LeftMouseDownPos = { 0, 0 };
    POINT RightDragStartPos = { 0, 0 };
    POINT RightMouseDownPos = { 0, 0 };

    // Scrolling
    int ScrollDelta = 0;
    int PrevScrollDelta = 0;

    // Window handle for focus check
    HWND OwnerHWnd = nullptr;

    // GUI InputState
    FGuiInputState GuiState{};
    FInputSystemSnapshot CurrentSnapshot{};
    bool bWindowFocused = true;

    static constexpr int DRAG_THRESHOLD = 5;

    // Internal drag threshold helper — unified Left/Right logic
    void FilterDragThreshold(
        bool& bCandidate, bool& bDragging, bool& bJustStarted,
        const POINT& MouseDownPos, POINT& DragStartPos);
    void PollGamepads();
    void ResetGamepadInputState();
    void UpdateCurrentSnapshot();
    void ResetDragState();
};
