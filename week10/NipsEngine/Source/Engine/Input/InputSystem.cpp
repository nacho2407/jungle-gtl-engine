#include "Engine/Input/InputSystem.h"
#include <cmath>

void InputSystem::Tick()
{
    // 윈도우 포커스가 없으면 모든 입력 상태 해제
    if (OwnerHWnd && GetForegroundWindow() != OwnerHWnd)
    {
        HandleOutOfFocusTick();
        return;
    }

    SampleKeyStates();

    bLeftDragJustStarted = false;
    bMiddleDragJustStarted = false;
    bRightDragJustStarted = false;
    bLeftDragJustEnded = false;
    bMiddleDragJustEnded = false;
    bRightDragJustEnded = false;

    PrevScrollDelta = ScrollDelta;
    ScrollDelta = 0;

    SampleMouseDelta();

    if (GetKeyDown(VK_LBUTTON))
    {
        bLeftDragCandidate = true;
        LeftMouseDownPos = MousePos;
    }

    if (GetKeyDown(VK_MBUTTON))
    {
        bMiddleDragCandidate = true;
        MiddleMouseDownPos = MousePos;
    }

    if (GetKeyDown(VK_RBUTTON))
    {
        bRightDragCandidate = true;
        RightMouseDownPos = MousePos;
    }

    // Left drag
    if (!bLeftDragging && IsDraggingLeft())
    {
        FilterDragThreshold(bLeftDragCandidate, bLeftDragging, bLeftDragJustStarted, LeftMouseDownPos,
                            LeftDragStartPos);
    }
    else if (GetKeyUp(VK_LBUTTON))
    {
        if (bLeftDragging)
            bLeftDragJustEnded = true;
        bLeftDragging = false;
        bLeftDragCandidate = false;
    }

    // Middle drag
    if (!bMiddleDragging && IsDraggingMiddle())
    {
        FilterDragThreshold(bMiddleDragCandidate, bMiddleDragging, bMiddleDragJustStarted, MiddleMouseDownPos,
                            MiddleDragStartPos);
    }
    else if (GetKeyUp(VK_MBUTTON))
    {
        if (bMiddleDragging)
            bMiddleDragJustEnded = true;
        bMiddleDragging = false;
        bMiddleDragCandidate = false;
    }

    // Right drag
    if (!bRightDragging && IsDraggingRight())
    {
        FilterDragThreshold(bRightDragCandidate, bRightDragging, bRightDragJustStarted, RightMouseDownPos,
                            RightDragStartPos);
    }
    else if (GetKeyUp(VK_RBUTTON))
    {
        if (bRightDragging)
            bRightDragJustEnded = true;
        bRightDragging = false;
        bRightDragCandidate = false;
    }
}

FInputSystemSnapshot InputSystem::TickAndMakeSnapshot()
{
    Tick();
    return MakeSnapshot();
}

FInputSystemSnapshot InputSystem::MakeSnapshot() const
{
    FInputSystemSnapshot Snapshot{};
    for (int VK = 0; VK < 256; ++VK)
    {
        Snapshot.KeyDown[VK] = CurrentStates[VK];
        Snapshot.KeyPressed[VK] = CurrentStates[VK] && !PrevStates[VK];
        Snapshot.KeyReleased[VK] = !CurrentStates[VK] && PrevStates[VK];
    }

    Snapshot.MousePos = MousePos;
    Snapshot.MouseDeltaX = FrameMouseDeltaX;
    Snapshot.MouseDeltaY = FrameMouseDeltaY;
    Snapshot.ScrollDelta = PrevScrollDelta;

    Snapshot.bLeftDragStarted = bLeftDragJustStarted;
    Snapshot.bLeftDragging = bLeftDragging;
    Snapshot.bLeftDragEnded = bLeftDragJustEnded;
    Snapshot.LeftDragVector = GetLeftDragVector();

    Snapshot.bMiddleDragStarted = bMiddleDragJustStarted;
    Snapshot.bMiddleDragging = bMiddleDragging;
    Snapshot.bMiddleDragEnded = bMiddleDragJustEnded;
    Snapshot.MiddleDragVector = GetMiddleDragVector();

    Snapshot.bRightDragStarted = bRightDragJustStarted;
    Snapshot.bRightDragging = bRightDragging;
    Snapshot.bRightDragEnded = bRightDragJustEnded;
    Snapshot.RightDragVector = GetRightDragVector();

    Snapshot.bUsingRawMouse = bUseRawMouse;
    return Snapshot;
}

void InputSystem::HandleOutOfFocusTick()
{
    switch (FocusLossPolicy)
    {
    case EInputFocusLossPolicy::ResetAllInputs:
    default:
        ResetAllInputStateOnFocusLoss();
        return;
    }
}

void InputSystem::ResetAllInputStateOnFocusLoss()
{
    if (bIsMouseLocked)
    {
        LockMouse(false);
    }
    SetCursorVisibility(true);

    for (int i = 0; i < 256; ++i)
    {
        PrevStates[i] = CurrentStates[i];
        CurrentStates[i] = false;
    }

    bUseRawMouse = false;

    bLeftDragJustStarted = false;
    bMiddleDragJustStarted = false;
    bRightDragJustStarted = false;

    bLeftDragJustEnded = bLeftDragging;
    bMiddleDragJustEnded = bMiddleDragging;
    bRightDragJustEnded = bRightDragging;

    bLeftDragging = false;
    bMiddleDragging = false;
    bRightDragging = false;
    bLeftDragCandidate = false;
    bMiddleDragCandidate = false;
    bRightDragCandidate = false;

    PrevScrollDelta = ScrollDelta;
    ScrollDelta = 0;
    FrameMouseDeltaX = 0;
    FrameMouseDeltaY = 0;
    RawMouseDeltaAccumX = 0;
    RawMouseDeltaAccumY = 0;

    GetCursorPos(&MousePos);
    PrevMousePos = MousePos;
}

void InputSystem::SampleKeyStates()
{
    // 단 한 번의 API 호출로 모든 키 상태 갱신 (CPU 10% 병목)
    BYTE KeyState[256];
    if (GetKeyboardState(KeyState))
    {
        for (int i = 0; i < 256; ++i)
        {
            PrevStates[i] = CurrentStates[i];
            CurrentStates[i] = (KeyState[i] & 0x80) != 0;
        }
    }
}

void InputSystem::SampleMouseDelta()
{
    PrevMousePos = MousePos;
    GetCursorPos(&MousePos);

    FrameMouseDeltaX = bIsMouseLocked ? MousePos.x - LockedCenterScreen.x : MousePos.x - PrevMousePos.x;
    FrameMouseDeltaY = bIsMouseLocked ? MousePos.y - LockedCenterScreen.y : MousePos.y - PrevMousePos.y;
    if (bUseRawMouse)
    {
        FrameMouseDeltaX = RawMouseDeltaAccumX;
        FrameMouseDeltaY = RawMouseDeltaAccumY;
    }

    RawMouseDeltaAccumX = 0;
    RawMouseDeltaAccumY = 0;

    if (bIsMouseLocked)
    {
        SetCursorPos(LockedCenterScreen.x, LockedCenterScreen.y);
    }
}

void InputSystem::AddRawMouseDelta(int DeltaX, int DeltaY)
{
    RawMouseDeltaAccumX += DeltaX;
    RawMouseDeltaAccumY += DeltaY;
}

void InputSystem::AddTextInput(uint32_t Codepoint)
{
    if (Codepoint == 0)
    {
        return;
    }

    TextInputQueue.push_back(Codepoint);
}

std::vector<uint32_t> InputSystem::ConsumeTextInput()
{
    std::vector<uint32_t> Result;
    Result.swap(TextInputQueue);
    return Result;
}

void InputSystem::FilterDragThreshold(bool& bCandidate, bool& bDragging, bool& bJustStarted, const POINT& MouseDownPos,
                                      POINT& DragStartPos)
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

float InputSystem::GetLeftDragDistance() const
{
    POINT V = GetLeftDragVector();
    return std::sqrt((float)(V.x * V.x + V.y * V.y));
}

POINT InputSystem::GetMiddleDragVector() const
{
    POINT V;
    V.x = MousePos.x - MiddleDragStartPos.x;
    V.y = MousePos.y - MiddleDragStartPos.y;
    return V;
}

float InputSystem::GetMiddleDragDistance() const
{
    POINT V = GetMiddleDragVector();
    return std::sqrt((float)V.x * V.x + V.y * V.y);
}

POINT InputSystem::GetRightDragVector() const
{
    POINT V;
    V.x = MousePos.x - RightDragStartPos.x;
    V.y = MousePos.y - RightDragStartPos.y;
    return V;
}

float InputSystem::GetRightDragDistance() const
{
    POINT V = GetRightDragVector();
    return std::sqrt((float)(V.x * V.x + V.y * V.y));
}

// --- Mouse lock ------------------------------------------------
void InputSystem::LockMouse(bool bLock, float x, float y, float w, float h)
{
    if (bLock)
    {
        if (!bIsMouseLocked)
        {
            GetCursorPos(&MouseLockRestoreScreen);
            bHasMouseLockRestoreScreen = true;
        }

        bIsMouseLocked = true;
        auto WarpX = x + w * 0.5f;
		auto WarpY = y + h * 0.5f;
        LockedCenterScreen = {(LONG)WarpX, (LONG)WarpY};
        SetCursorPos(LockedCenterScreen.x, LockedCenterScreen.y);
        MousePos = LockedCenterScreen;
        PrevMousePos = LockedCenterScreen;
        FrameMouseDeltaX = 0;
        FrameMouseDeltaY = 0;
        RawMouseDeltaAccumX = 0;
        RawMouseDeltaAccumY = 0;
        return;
    }

    const bool bWasLocked = bIsMouseLocked;
    bIsMouseLocked = false;
    ClipCursor(nullptr);

    if (bWasLocked && bHasMouseLockRestoreScreen)
    {
        SetCursorPos(MouseLockRestoreScreen.x, MouseLockRestoreScreen.y);
        MousePos = MouseLockRestoreScreen;
        PrevMousePos = MouseLockRestoreScreen;
    }
    else
    {
        GetCursorPos(&MousePos);
        PrevMousePos = MousePos;
    }

    bHasMouseLockRestoreScreen = false;
    FrameMouseDeltaX = 0;
    FrameMouseDeltaY = 0;
    RawMouseDeltaAccumX = 0;
    RawMouseDeltaAccumY = 0;
}

void InputSystem::SetCursorVisibility(bool bVisible)
{
    if (bVisible)
    {
        while (ShowCursor(TRUE) < 0) {}
        while (ShowCursor(FALSE) >= 0) {}
        ShowCursor(TRUE);
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        bIsCursorVisible = true;
    }
    else
    {
        while (ShowCursor(TRUE) < 0) {}
        while (ShowCursor(FALSE) >= 0) {}
        ShowCursor(TRUE);
        ShowCursor(FALSE);
        bIsCursorVisible = false;
    }
}
