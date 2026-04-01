#pragma once
#include <windows.h>
#include "Singleton.h"
#include "Runtime/ViewportRect.h"

struct FGuiInputState
{
    bool bUsingMouse = false;
    bool bUsingKeyboard = false;

	bool bViewportHostVisible = false;
	FViewportRect ViewportHostRect;

	bool IsInViewportHost(int32 X, int32 Y) const
	{
		return bViewportHostVisible && ViewportHostRect.Contains(X, Y);
	}
};



class InputSystem : public TSingleton<InputSystem>
{
	friend class TSingleton<InputSystem>;

public:
    void Tick();

    // Keyboard
    bool GetKeyDown(int VK) const { return CurrentStates[VK] && !PrevStates[VK]; }
    bool GetKey(int VK) const { return CurrentStates[VK]; }
    bool GetKeyUp(int VK) const { return !CurrentStates[VK] && PrevStates[VK]; }

    // Mouse position
    POINT GetMousePos() const { return MousePos; }
    int MouseDeltaX() const { return MousePos.x - PrevMousePos.x; }
    int MouseDeltaY() const { return MousePos.y - PrevMousePos.y; }
    bool MouseMoved() const { return MouseDeltaX() != 0 || MouseDeltaY() != 0; }

    // Left drag
    bool IsDraggingLeft() const { return GetKey(VK_LBUTTON) && MouseMoved(); }
    bool GetLeftDragStart() const { return bLeftDragJustStarted; }
    bool GetLeftDragging() const { return bLeftDragging; }
    bool GetLeftDragEnd() const { return bLeftDragJustEnded; }
    POINT GetLeftDragVector() const;
    float GetLeftDragDistance() const;

	// Middle drag
	bool IsDraggingMiddle() const { return GetKey(VK_MBUTTON) && MouseMoved(); }
	bool GetMiddleDragStart() const { return bMiddleDragJustStarted; }
	bool GetMiddleDragging() const { return bMiddleDragging; }
	bool GetMiddleDragEnd() const { return bMiddleDragJustEnded; }
	POINT GetMiddleDragVector() const;
	float GetMiddleDragDistance() const;

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

private:
    bool CurrentStates[256] = { false };
    bool PrevStates[256] = { false };

    // Mouse members
    POINT MousePos = { 0, 0 };
    POINT PrevMousePos = { 0, 0 };

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
    POINT LeftDragStartPos = { 0, 0 };
    POINT LeftMouseDownPos = { 0, 0 };
	POINT MiddleDragStartPos = { 0, 0 };
    POINT MiddleMouseDownPos = { 0, 0 };
    POINT RightDragStartPos = { 0, 0 };
    POINT RightMouseDownPos = { 0, 0 };

    // Scrolling
    int ScrollDelta = 0;
    int PrevScrollDelta = 0;

    // Window handle for focus check
    HWND OwnerHWnd = nullptr;

    // GUI InputState
    FGuiInputState GuiState{};

    static constexpr int DRAG_THRESHOLD = 5;

    // Internal drag threshold helper — unified Left/Right logic
    void FilterDragThreshold(
        bool& bCandidate, bool& bDragging, bool& bJustStarted,
        const POINT& MouseDownPos, POINT& DragStartPos);
};
