#pragma once
#include <windows.h>
#include <cstdint>
#include <vector>
#include "Core/Singleton.h"
#include "Runtime/ViewportRect.h"

struct FGuiInputState
{
    bool bUsingMouse = false;
    bool bUsingKeyboard = false;
    bool bUsingTextInput = false;
    bool bBlockViewportMouse = false;
    bool bAllowViewportMouseFocus = false;

    bool          bViewportHostVisible = false;
    FViewportRect ViewportHostRect;

    bool IsInViewportHost(int32 X, int32 Y) const { return bViewportHostVisible && ViewportHostRect.Contains(X, Y); }
};

struct FInputSystemSnapshot
{
    bool KeyDown[256] = {};
    bool KeyPressed[256] = {};
    bool KeyReleased[256] = {};

    POINT MousePos = { 0, 0 };
    int   MouseDeltaX = 0;
    int   MouseDeltaY = 0;
    int   ScrollDelta = 0;

    bool  bLeftDragStarted = false;
    bool  bLeftDragging = false;
    bool  bLeftDragEnded = false;
    POINT LeftDragVector = { 0, 0 };

    bool  bMiddleDragStarted = false;
    bool  bMiddleDragging = false;
    bool  bMiddleDragEnded = false;
    POINT MiddleDragVector = { 0, 0 };

    bool  bRightDragStarted = false;
    bool  bRightDragging = false;
    bool  bRightDragEnded = false;
    POINT RightDragVector = { 0, 0 };

    bool bUsingRawMouse = false;

    bool IsDown(int VK) const { return VK >= 0 && VK < 256 && KeyDown[VK]; }
    bool WasPressed(int VK) const { return VK >= 0 && VK < 256 && KeyPressed[VK]; }
    bool WasReleased(int VK) const { return VK >= 0 && VK < 256 && KeyReleased[VK]; }
};

enum class EInputFocusLossPolicy : uint8
{
    ResetAllInputs
};

class InputSystem : public TSingleton<InputSystem>
{
    friend class TSingleton<InputSystem>;

  public:
    void Tick();
    FInputSystemSnapshot TickAndMakeSnapshot();
    FInputSystemSnapshot MakeSnapshot() const;
    void SetUseRawMouse(bool bEnable) { bUseRawMouse = bEnable; }
    bool IsUsingRawMouse() const { return bUseRawMouse; }
    void AddRawMouseDelta(int DeltaX, int DeltaY);
    void AddTextInput(uint32_t Codepoint);
    std::vector<uint32_t> ConsumeTextInput();
    std::vector<uint32_t> ConsumeScriptTextInput();

    // Keyboard
    bool GetKeyDown(int VK) const { return CurrentStates[VK] && !PrevStates[VK]; }
    bool GetKey(int VK) const { return CurrentStates[VK]; }
    bool GetKeyUp(int VK) const { return !CurrentStates[VK] && PrevStates[VK]; }
    bool IsAnyMouseButtonDown() const
    {
        return GetKey(VK_LBUTTON)
            || GetKey(VK_RBUTTON)
            || GetKey(VK_MBUTTON)
            || GetKey(VK_XBUTTON1)
            || GetKey(VK_XBUTTON2);
    }
    bool IsAnyMouseButtonDownOrDragging() const
    {
        return IsAnyMouseButtonDown()
            || GetLeftDragging()
            || GetMiddleDragging()
            || GetRightDragging();
    }

	// Mouse lock
    void	 SetCursorVisibility(bool bVisible);
    void	 LockMouse(bool bLock, float x = 0, float y = 0, float w = 0, float h = 0);

    // Mouse position
    POINT GetMousePos() const { return MousePos; }
    int MouseDeltaX() const { return FrameMouseDeltaX; }
	int MouseDeltaY() const { return FrameMouseDeltaY; }
    bool  MouseMoved() const { return MouseDeltaX() != 0 || MouseDeltaY() != 0; }

    // Left drag
    bool  IsDraggingLeft() const { return GetKey(VK_LBUTTON) && MouseMoved(); }
    bool  GetLeftDragStart() const { return bLeftDragJustStarted; }
    bool  GetLeftDragging() const { return bLeftDragging; }
    bool  GetLeftDragEnd() const { return bLeftDragJustEnded; }
    POINT GetLeftDragVector() const;
    float GetLeftDragDistance() const;

    // Middle drag
    bool  IsDraggingMiddle() const { return GetKey(VK_MBUTTON) && MouseMoved(); }
    bool  GetMiddleDragStart() const { return bMiddleDragJustStarted; }
    bool  GetMiddleDragging() const { return bMiddleDragging; }
    bool  GetMiddleDragEnd() const { return bMiddleDragJustEnded; }
    POINT GetMiddleDragVector() const;
    float GetMiddleDragDistance() const;

    // Right drag
    bool  IsDraggingRight() const { return GetKey(VK_RBUTTON) && MouseMoved(); }
    bool  GetRightDragStart() const { return bRightDragJustStarted; }
    bool  GetRightDragging() const { return bRightDragging; }
    bool  GetRightDragEnd() const { return bRightDragJustEnded; }
    POINT GetRightDragVector() const;
    float GetRightDragDistance() const;

    // Scrolling
    void  AddScrollDelta(int Delta) { ScrollDelta += Delta; }
    int   GetScrollDelta() const { return PrevScrollDelta; }
    bool  ScrolledUp() const { return PrevScrollDelta > 0; }
    bool  ScrolledDown() const { return PrevScrollDelta < 0; }
    float GetScrollNotches() const { return PrevScrollDelta / (float)WHEEL_DELTA; }

    // Window focus
    void SetOwnerWindow(HWND InHWnd) { OwnerHWnd = InHWnd; }
    void SetFocusLossPolicy(EInputFocusLossPolicy InPolicy) { FocusLossPolicy = InPolicy; }

    // UI capture declarations consumed by the input policy setup.
    FGuiInputState&       GetGuiInputState() { return GuiState; }
    const FGuiInputState& GetGuiInputState() const { return GuiState; }
    void SetGuiMouseCapture(bool bCapture) { GuiState.bUsingMouse = bCapture; }
    void SetGuiKeyboardCapture(bool bCapture) { GuiState.bUsingKeyboard = bCapture; }
    void SetGuiTextInputCapture(bool bCapture) { GuiState.bUsingTextInput = bCapture; }
    void SetGuiViewportMouseBlock(bool bBlock) { GuiState.bBlockViewportMouse = bBlock; }
    void SetGuiViewportMouseFocusAllowed(bool bAllow) { GuiState.bAllowViewportMouseFocus = bAllow; }

  private:
    bool CurrentStates[256] = {false};
    bool PrevStates[256] = {false};

    // Mouse members
    POINT MousePos = {0, 0};
    POINT PrevMousePos = {0, 0};
    int   FrameMouseDeltaX = 0;
    int   FrameMouseDeltaY = 0;
    int   RawMouseDeltaAccumX = 0;
    int   RawMouseDeltaAccumY = 0;
    bool  bUseRawMouse = false;
    POINT LockedCenterScreen;
    POINT MouseLockRestoreScreen = {0, 0};
	bool  bIsMouseLocked = false;
    bool  bHasMouseLockRestoreScreen = false;
    bool  bIsCursorVisible = false;

    bool bLeftDragCandidate = false;
    bool bMiddleDragCandidate = false;
    bool bRightDragCandidate = false;
    bool bLeftDragging = false;
    bool bMiddleDragging = false;
    bool bRightDragging = false;

    bool bLeftDragJustStarted = false;
    bool bMiddleDragJustStarted = false;
    bool bRightDragJustStarted = false;
    bool bLeftDragJustEnded = false;
    bool bMiddleDragJustEnded = false;
    bool bRightDragJustEnded = false;

    // Drag origin
    POINT LeftDragStartPos = {0, 0};
    POINT LeftMouseDownPos = {0, 0};
    POINT MiddleDragStartPos = {0, 0};
    POINT MiddleMouseDownPos = {0, 0};
    POINT RightDragStartPos = {0, 0};
    POINT RightMouseDownPos = {0, 0};

    // Scrolling
    int ScrollDelta = 0;
    int PrevScrollDelta = 0;
    std::vector<uint32_t> TextInputQueue;
    std::vector<uint32_t> ScriptTextInputQueue;

    // Window handle for focus check
    HWND OwnerHWnd = nullptr;
    EInputFocusLossPolicy FocusLossPolicy = EInputFocusLossPolicy::ResetAllInputs;
    bool bHadInputFocus = true;

    // UI capture state
    FGuiInputState GuiState{};

    static constexpr int DRAG_THRESHOLD = 5;

    // Internal drag threshold helper — unified Left/Right logic
    void FilterDragThreshold(bool& bCandidate, bool& bDragging, bool& bJustStarted, const POINT& MouseDownPos,
                             POINT& DragStartPos);
    void HandleOutOfFocusTick();
    void ResetAllInputStateOnFocusLoss();
    void SampleKeyStates();
    void SampleMouseDelta();
};
