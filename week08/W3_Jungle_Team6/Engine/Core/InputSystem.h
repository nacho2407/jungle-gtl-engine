#pragma once
#include <windows.h>

struct FGuiInputState
{
    bool bUsingMouse = false;
    bool bUsingKeyboard = false;
};

class InputSystem {
private:
public:
    static bool currentStates[256];
    static bool prevStates[256];

    // Mouse members
    static POINT mousePos;
    static POINT prevMousePos;
    static bool leftDragCandidate;
    static bool rightDragCandidate;
    static bool leftDragging;
    static bool rightDragging;
    static const int DRAG_THRESHOLD = 5;

    static bool leftDragJustStarted;
    static bool rightDragJustStarted;
    static bool leftDragJustEnded;
    static bool rightDragJustEnded;

    // Drag origin and destination
    static POINT leftDragStartPos;
    static POINT leftMouseDownPos;
    static POINT rightDragStartPos;
    static POINT rightMouseDownPos;

    // Scrolling
    static int scrollDelta;      // accumulated this frame
    static int prevScrollDelta;

    //UI InputState
    static FGuiInputState GuiInputState;


    //__________________________________________________________________________________________


    static void Update() {
        for (int i = 0; i < 256; ++i) {
            prevStates[i] = currentStates[i];
            currentStates[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
        }

        leftDragJustStarted = false;
        rightDragJustStarted = false;
        leftDragJustEnded = false;
        rightDragJustEnded = false;

        prevScrollDelta = scrollDelta;
        scrollDelta = 0;

        prevMousePos = mousePos;
        GetCursorPos(&mousePos);

        if (GetKeyDown(VK_LBUTTON)) {
            leftDragCandidate = true;
            leftMouseDownPos = mousePos;
        }
        if (GetKeyDown(VK_RBUTTON)) {
            rightDragCandidate = true;
            rightMouseDownPos = mousePos;
        }

        if (!leftDragging && IsDraggingLeft()) {
            FilterDragThresholdLeft();
        }
        else {
            if (GetKeyUp(VK_LBUTTON)) {
                if (leftDragging) leftDragJustEnded = true;
                leftDragging = false;
                leftDragCandidate = false;
            }
        }
        if (!rightDragging && IsDraggingRight()) {
            FilterDragThresholdRight();
        }
        else {
            if (GetKeyUp(VK_RBUTTON)) {
                if (rightDragging) rightDragJustEnded = true;
                rightDragging = false;
                rightDragCandidate = false;
            }
        }
    }

    static void UpdateMousePosition(int x, int y) {
        prevMousePos = mousePos;
        mousePos.x = x;
        mousePos.y = y;
    }

    static bool GetKeyDown(int vk) { return currentStates[vk] && !prevStates[vk]; }
    static bool GetKey(int vk) { return currentStates[vk]; }
    static bool GetKeyUp(int vk) { return !currentStates[vk] && prevStates[vk]; }


    // Mouse func
    static int MouseDeltaX() { return mousePos.x - prevMousePos.x; }
    static int MouseDeltaY() { return mousePos.y - prevMousePos.y; }
    static float MouseDeltaXPerSecond(float DeltaTime) 
    { 
        return DeltaTime > 1e-6f ? (mousePos.x - prevMousePos.x) / DeltaTime : 0.0f;
    }
    static float MouseDeltaYPerSecond(float DeltaTime)
    {
        return DeltaTime > 1e-6f ? (mousePos.y - prevMousePos.y) / DeltaTime : 0.0f;
    }
    static bool mouseMoved() { return MouseDeltaX() != 0 || MouseDeltaY() != 0; }

    static bool IsDraggingLeft() {
        return GetKey(VK_LBUTTON) && mouseMoved();
    }
    static bool GetLeftDragStart() {
        return leftDragJustStarted;
    }

    static bool GetLeftDragging() {
        return leftDragging;
    }

    static bool GetLeftDragEnd() { return leftDragJustEnded; }

    static bool IsDraggingRight() {
        return GetKey(VK_RBUTTON) && mouseMoved();
    }
    static bool GetRightDragStart() {
        return rightDragJustStarted;
    }

    static bool GetRightDragging() {
        return rightDragging;
    }

    static bool GetRightDragEnd() { return rightDragJustEnded; }

    static void FilterDragThresholdLeft();
    static void FilterDragThresholdRight();

    // Drag vectors
    static POINT GetLeftDragVector();
    static float GetLeftDragDistance();
    static POINT GetRightDragVector();
    static float GetRightDragDistance();

    // Scrolling
    static void AddScrollDelta(int delta) { scrollDelta += delta; }
    static int GetScrollDelta() { return prevScrollDelta; }
    static bool ScrolledUp() { return prevScrollDelta > 0; }
    static bool ScrolledDown() { return prevScrollDelta < 0; }
    static float GetScrollNotches() { return prevScrollDelta / (float)WHEEL_DELTA; }
};