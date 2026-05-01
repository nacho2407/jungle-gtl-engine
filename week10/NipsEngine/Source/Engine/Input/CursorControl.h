#pragma once

#include <windows.h>

struct FCursorControlState
{
    bool  bHideInClient = false;
    bool  bLockToScreenPos = false;
    POINT LockScreenPos = { 0, 0 };
    HWND  OwnerWindow = nullptr;
};

class FCursorControl
{
public:
    static void SetState(const FCursorControlState& InState)
    {
        GState = InState;
        ApplyPlatformState(GState);
    }

    static FCursorControlState GetState()
    {
        return GState;
    }

    static void Apply()
    {
        ApplyPlatformState(GState);
    }

    static bool IsCursorHidden()
    {
        return bCursorHidden;
    }

    static void Clear()
    {
        const bool bHadCursorOwner = GState.bHideInClient || GState.bLockToScreenPos || GState.OwnerWindow != nullptr;
        GState = FCursorControlState{};
        if (bCursorHidden || bHadCursorOwner)
        {
            SetCursorHidden(false);
        }
        ::ClipCursor(nullptr);
    }

private:
    static inline FCursorControlState GState{};
    static inline bool bCursorHidden = false;

    static void NormalizeCursorCounterVisible()
    {
        while (::ShowCursor(TRUE) < 0) {}
        while (::ShowCursor(FALSE) >= 0) {}
        ::ShowCursor(TRUE);
    }

    static void NormalizeCursorCounterHidden()
    {
        NormalizeCursorCounterVisible();
        ::ShowCursor(FALSE);
    }

    static void RestoreArrowCursor()
    {
        ::SetCursor(::LoadCursorW(nullptr, IDC_ARROW));
    }

    static void SetCursorHidden(bool bHide)
    {
        if (bHide)
        {
            NormalizeCursorCounterHidden();
        }
        else
        {
            NormalizeCursorCounterVisible();
            RestoreArrowCursor();
        }
        bCursorHidden = bHide;
    }

    static void ApplyPlatformState(const FCursorControlState& State)
    {
        const bool bOwnerForeground = State.OwnerWindow != nullptr && ::GetForegroundWindow() == State.OwnerWindow;
        const bool bOwnerCaptured = State.OwnerWindow != nullptr && ::GetCapture() == State.OwnerWindow;
        const bool bHasValidOwner = bOwnerForeground || bOwnerCaptured;
        const bool bShouldHideCursor = State.bHideInClient && bHasValidOwner;

        if (bShouldHideCursor != bCursorHidden)
        {
            SetCursorHidden(bShouldHideCursor);
        }

        if (!bShouldHideCursor)
        {
            ::ClipCursor(nullptr);
            return;
        }

        ::SetCursor(nullptr);
        if (State.bLockToScreenPos)
        {
            const POINT LockPos = State.LockScreenPos;
            const RECT  LockRect = { LockPos.x, LockPos.y, LockPos.x + 1, LockPos.y + 1 };
            ::SetCursorPos(LockPos.x, LockPos.y);
            ::ClipCursor(&LockRect);
        }
    }
};
