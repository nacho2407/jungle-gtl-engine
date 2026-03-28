#include "Engine/Core/InputSystem.h"
#include <cmath>

void InputSystem::Tick()
{
    // 윈도우 포커스가 없으면 모든 입력 상태 해제
    if (OwnerHWnd && GetForegroundWindow() != OwnerHWnd)
    {
        for (int i = 0; i < 256; ++i)
        {
            PrevStates[i] = CurrentStates[i];
            CurrentStates[i] = false;
        }
        bLeftDragJustStarted = false;
        bRightDragJustStarted = false;
        bLeftDragJustEnded = bLeftDragging;
        bRightDragJustEnded = bRightDragging;
        bLeftDragging = false;
        bRightDragging = false;
        bLeftDragCandidate = false;
        bRightDragCandidate = false;
        PrevScrollDelta = ScrollDelta;
        ScrollDelta = 0;
        // 마우스 위치 동기화 (복귀 시 델타 점프 방지)
        GetCursorPos(&MousePos);
        PrevMousePos = MousePos;
        return;
    }
	
    // 단 한 번의 API 호출로 모든 키 상태 갱신 (CPU 10% 병목)
	BYTE keyState[256];
    if (GetKeyboardState(keyState))
    {
        for (int i = 0; i < 256; ++i)
        {
            PrevStates[i] = CurrentStates[i];
            // GetKeyboardState는 최상위 비트(0x80)가 눌림 상태를 나타냄
            CurrentStates[i] = (keyState[i] & 0x80) != 0; 
        }
    }

    bLeftDragJustStarted = false;
    bRightDragJustStarted = false;
    bLeftDragJustEnded = false;
    bRightDragJustEnded = false;

    PrevScrollDelta = ScrollDelta;
    ScrollDelta = 0;

    PrevMousePos = MousePos;
    GetCursorPos(&MousePos);

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
