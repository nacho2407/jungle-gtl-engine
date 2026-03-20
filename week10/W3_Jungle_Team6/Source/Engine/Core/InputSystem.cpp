#include "Engine/Core/InputSystem.h"
#include <cmath>

// Keyboard definition
bool InputSystem::currentStates[256] = { false };
bool InputSystem::prevStates[256] = { false };

// Mouse definition
POINT InputSystem::mousePos = { 0,0 };
POINT InputSystem::prevMousePos = { 0,0 };
bool InputSystem::leftDragCandidate = false;
bool InputSystem::rightDragCandidate = false;
POINT InputSystem::leftDragStartPos = { 0, 0 };
POINT InputSystem::leftMouseDownPos = { 0, 0 };
POINT InputSystem::rightDragStartPos = { 0, 0 };
POINT InputSystem::rightMouseDownPos = { 0, 0 };
bool InputSystem::leftDragging = false;
bool InputSystem::rightDragging = false;
bool InputSystem::leftDragJustStarted = false;
bool InputSystem::rightDragJustStarted = false;
bool InputSystem::leftDragJustEnded = false;
bool InputSystem::rightDragJustEnded = false;
int InputSystem::scrollDelta = 0;
int InputSystem::prevScrollDelta = 0;


FGuiInputState InputSystem::GuiInputState{};


void InputSystem::FilterDragThresholdLeft() {
    if (leftDragCandidate && !leftDragging)
    {
        int dx = mousePos.x - leftMouseDownPos.x;
        int dy = mousePos.y - leftMouseDownPos.y;

        int distSq = dx * dx + dy * dy;

        if (distSq >= DRAG_THRESHOLD * DRAG_THRESHOLD)
        {
            leftDragJustStarted = true;
            leftDragging = true;
            leftDragStartPos = leftMouseDownPos;
        }
    }
}

void InputSystem::FilterDragThresholdRight() {
    if (rightDragCandidate && !rightDragging)
    {
        int dx = mousePos.x - rightMouseDownPos.x;
        int dy = mousePos.y - rightMouseDownPos.y;

        int distSq = dx * dx + dy * dy;

        if (distSq >= DRAG_THRESHOLD * DRAG_THRESHOLD)
        {
            rightDragJustStarted = true;
            rightDragging = true;
            rightDragStartPos = rightMouseDownPos;
        }
    }
}


POINT InputSystem::GetLeftDragVector()
{
    POINT v;
    v.x = mousePos.x - leftDragStartPos.x;
    v.y = mousePos.y - leftDragStartPos.y;
    return v;
}
POINT InputSystem::GetRightDragVector()
{
    POINT v;
    v.x = mousePos.x - rightDragStartPos.x;
    v.y = mousePos.y - rightDragStartPos.y;
    return v;
}

float InputSystem::GetLeftDragDistance()
{
    POINT v = GetLeftDragVector();
    return std::sqrt((float)(v.x * v.x + v.y * v.y));
}

float InputSystem::GetRightDragDistance()
{
    POINT v = GetRightDragVector();
    return std::sqrt((float)(v.x * v.x + v.y * v.y));
}