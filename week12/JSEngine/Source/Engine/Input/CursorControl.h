#pragma once

#include <windows.h>

#include "Engine/Core/Logging/Log.h"
#include "Engine/Input/InputWindowFocus.h"

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

    static int32 LogShowCursorCall(const char* Caller, BOOL bShow)
    {
        const int32 Result = ::ShowCursor(bShow);
        UE_LOG("[CursorDebug] ShowCursor Caller=%s Arg=%s Result=%d",
            Caller,
            bShow ? "TRUE" : "FALSE",
            Result);
        return Result;
    }

    static void NormalizeCursorCounterVisible()
    {
        while (LogShowCursorCall("FCursorControl::NormalizeCursorCounterVisible", TRUE) < 0) {}
        while (LogShowCursorCall("FCursorControl::NormalizeCursorCounterVisible", FALSE) >= 0) {}
        LogShowCursorCall("FCursorControl::NormalizeCursorCounterVisible", TRUE);
    }

    static void NormalizeCursorCounterHidden()
    {
        NormalizeCursorCounterVisible();
        LogShowCursorCall("FCursorControl::NormalizeCursorCounterHidden", FALSE);
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
        const bool bOwnerForeground = State.OwnerWindow != nullptr && InputWindowFocus::IsForegroundWindowOwnedByCurrentProcess();
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
