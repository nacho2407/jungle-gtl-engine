#include "Engine/Input/InputRouter.h"

#include <algorithm>
#include <utility>

#include "Engine/Input/CursorControl.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/Viewport.h"
#include "Engine/Runtime/ViewportClient.h"

namespace
{
constexpr float ViewportInputDeadZonePixels = 4.0f;

struct FPointerButtonsState
{
    bool bAnyPressed = false;
    bool bAnyDown = false;
};

bool IsMouseButtonKey(int32 VK)
{
    return (VK == VK_LBUTTON) || (VK == VK_RBUTTON) || (VK == VK_MBUTTON)
        || (VK == VK_XBUTTON1) || (VK == VK_XBUTTON2);
}

EPointerButton ToPointerButton(int32 VK)
{
    switch (VK)
    {
    case VK_LBUTTON: return EPointerButton::Left;
    case VK_RBUTTON: return EPointerButton::Right;
    case VK_MBUTTON: return EPointerButton::Middle;
    default: return EPointerButton::None;
    }
}

FRect GetInsetRect(const FRect& Rect, float Inset)
{
    FRect Out = Rect;
    Out.X += Inset;
    Out.Y += Inset;
    Out.Width -= Inset * 2.0f;
    Out.Height -= Inset * 2.0f;
    return Out;
}

POINT ClampPointToRect(POINT Point, const RECT& Rect)
{
    const LONG MinX = Rect.left;
    const LONG MinY = Rect.top;
    const LONG MaxX = std::max(Rect.left, Rect.right - 1);
    const LONG MaxY = std::max(Rect.top, Rect.bottom - 1);
    Point.x = std::clamp(Point.x, MinX, MaxX);
    Point.y = std::clamp(Point.y, MinY, MaxY);
    return Point;
}

void AppendKeyEvents(const FInputSystemSnapshot& Snapshot, const POINT& MouseScreenPos, const POINT& MouseDelta, TArray<FInputEvent>& OutEvents)
{
    for (int32 VK = 0; VK < 256; ++VK)
    {
        if (Snapshot.WasPressed(VK))
        {
            FInputEvent E{};
            E.Type = EInputEventType::KeyPressed;
            E.Key = VK;
            E.PointerButton = ToPointerButton(VK);
            E.MouseScreenPos = MouseScreenPos;
            E.MouseDelta = MouseDelta;
            OutEvents.push_back(E);
        }
        if (Snapshot.WasReleased(VK))
        {
            FInputEvent E{};
            E.Type = EInputEventType::KeyReleased;
            E.Key = VK;
            E.PointerButton = ToPointerButton(VK);
            E.MouseScreenPos = MouseScreenPos;
            E.MouseDelta = MouseDelta;
            OutEvents.push_back(E);
        }
    }
}

void AppendWheelEvent(float WheelNotches, const POINT& MouseScreenPos, TArray<FInputEvent>& OutEvents)
{
    if (WheelNotches == 0.0f)
    {
        return;
    }

    FInputEvent E{};
    E.Type = EInputEventType::WheelScrolled;
    E.MouseScreenPos = MouseScreenPos;
    E.WheelNotches = WheelNotches;
    OutEvents.push_back(E);
}

void AppendDragEvents(const FInputSystemSnapshot& Snapshot, const POINT& MouseScreenPos, TArray<FInputEvent>& OutEvents)
{
    if (Snapshot.bLeftDragStarted)
    {
        FInputEvent E{};
        E.Type = EInputEventType::PointerDragStarted;
        E.PointerButton = EPointerButton::Left;
        E.MouseScreenPos = MouseScreenPos;
        E.MouseDelta = Snapshot.LeftDragVector;
        OutEvents.push_back(E);
    }
    if (Snapshot.bLeftDragEnded)
    {
        FInputEvent E{};
        E.Type = EInputEventType::PointerDragEnded;
        E.PointerButton = EPointerButton::Left;
        E.MouseScreenPos = MouseScreenPos;
        E.MouseDelta = Snapshot.LeftDragVector;
        OutEvents.push_back(E);
    }
    if (Snapshot.bRightDragStarted)
    {
        FInputEvent E{};
        E.Type = EInputEventType::PointerDragStarted;
        E.PointerButton = EPointerButton::Right;
        E.MouseScreenPos = MouseScreenPos;
        E.MouseDelta = Snapshot.RightDragVector;
        OutEvents.push_back(E);
    }
    if (Snapshot.bRightDragEnded)
    {
        FInputEvent E{};
        E.Type = EInputEventType::PointerDragEnded;
        E.PointerButton = EPointerButton::Right;
        E.MouseScreenPos = MouseScreenPos;
        E.MouseDelta = Snapshot.RightDragVector;
        OutEvents.push_back(E);
    }
    if (Snapshot.bMiddleDragStarted)
    {
        FInputEvent E{};
        E.Type = EInputEventType::PointerDragStarted;
        E.PointerButton = EPointerButton::Middle;
        E.MouseScreenPos = MouseScreenPos;
        E.MouseDelta = Snapshot.MiddleDragVector;
        OutEvents.push_back(E);
    }
    if (Snapshot.bMiddleDragEnded)
    {
        FInputEvent E{};
        E.Type = EInputEventType::PointerDragEnded;
        E.PointerButton = EPointerButton::Middle;
        E.MouseScreenPos = MouseScreenPos;
        E.MouseDelta = Snapshot.MiddleDragVector;
        OutEvents.push_back(E);
    }
}

void ApplyViewportBlockMask(bool bBlockKeyboardForViewport, bool bBlockMouseForViewport, FViewportInputContext& InOutContext)
{
    if (!(bBlockKeyboardForViewport || bBlockMouseForViewport))
    {
        return;
    }

    if (bBlockMouseForViewport)
    {
        InOutContext.Frame.KeyDown[VK_LBUTTON] = false;
        InOutContext.Frame.KeyDown[VK_RBUTTON] = false;
        InOutContext.Frame.KeyDown[VK_MBUTTON] = false;
        InOutContext.Frame.KeyDown[VK_XBUTTON1] = false;
        InOutContext.Frame.KeyDown[VK_XBUTTON2] = false;
        InOutContext.Frame.bLeftDragging = false;
        InOutContext.Frame.bMiddleDragging = false;
        InOutContext.Frame.bRightDragging = false;
        InOutContext.Frame.LeftDragVector = { 0, 0 };
        InOutContext.Frame.MiddleDragVector = { 0, 0 };
        InOutContext.Frame.RightDragVector = { 0, 0 };
        InOutContext.Frame.WheelNotches = 0.0f;
        InOutContext.Frame.MouseDelta = { 0, 0 };
        InOutContext.MouseLocalDelta = { 0, 0 };
    }

    InOutContext.Events.erase(
        std::remove_if(
            InOutContext.Events.begin(),
            InOutContext.Events.end(),
            [bBlockKeyboardForViewport, bBlockMouseForViewport](const FInputEvent& Event)
            {
                if (bBlockMouseForViewport)
                {
                    if (Event.Type == EInputEventType::WheelScrolled)
                    {
                        return true;
                    }
                    if (Event.Type == EInputEventType::PointerDragStarted || Event.Type == EInputEventType::PointerDragEnded)
                    {
                        return true;
                    }
                    if ((Event.Type == EInputEventType::KeyPressed || Event.Type == EInputEventType::KeyReleased) && IsMouseButtonKey(Event.Key))
                    {
                        return true;
                    }
                }

                if (bBlockKeyboardForViewport
                    && (Event.Type == EInputEventType::KeyPressed || Event.Type == EInputEventType::KeyReleased)
                    && !IsMouseButtonKey(Event.Key))
                {
                    return true;
                }
                return false;
            }),
        InOutContext.Events.end());
}

FInputFrame BuildFrameFromSnapshot(const FInputSystemSnapshot& Snapshot, uint64 FrameNumber, HWND SourceWindow)
{
    FInputFrame Frame{};
    Frame.FrameNumber = FrameNumber;
    Frame.SourceWindow = SourceWindow;
    Frame.MouseInputMode = EMouseInputMode::Absolute;
    Frame.MouseScreenPos = Snapshot.MousePos;
    Frame.MouseDelta = { Snapshot.MouseDeltaX, Snapshot.MouseDeltaY };
    Frame.WheelNotches = Snapshot.ScrollDelta / static_cast<float>(WHEEL_DELTA);
    Frame.bLeftDragging = Snapshot.bLeftDragging;
    Frame.bMiddleDragging = Snapshot.bMiddleDragging;
    Frame.bRightDragging = Snapshot.bRightDragging;
    Frame.LeftDragVector = Snapshot.LeftDragVector;
    Frame.MiddleDragVector = Snapshot.MiddleDragVector;
    Frame.RightDragVector = Snapshot.RightDragVector;
    for (int32 VK = 0; VK < 256; ++VK)
    {
        Frame.KeyDown[VK] = Snapshot.KeyDown[VK];
    }
    return Frame;
}

FPointerButtonsState ComputePointerButtonsState(const FInputSystemSnapshot& Snapshot)
{
    FPointerButtonsState State{};
    State.bAnyPressed =
        Snapshot.WasPressed(VK_LBUTTON)
        || Snapshot.WasPressed(VK_RBUTTON)
        || Snapshot.WasPressed(VK_MBUTTON);
    State.bAnyDown =
        Snapshot.IsDown(VK_LBUTTON)
        || Snapshot.IsDown(VK_RBUTTON)
        || Snapshot.IsDown(VK_MBUTTON)
        || Snapshot.bLeftDragging
        || Snapshot.bMiddleDragging
        || Snapshot.bRightDragging;
    return State;
}
}

void FInputRouter::SetImGuiCaptureState(bool bCaptureMouse, bool bCaptureKeyboard)
{
    bImGuiCaptureMouse = bCaptureMouse;
    bImGuiCaptureKeyboard = bCaptureKeyboard;
}

void FInputRouter::SetForceViewportMouseBlock(bool bEnable, bool bAllowFocusPress)
{
    bForceViewportMouseBlock = bEnable;
    bAllowViewportFocusPress = bAllowFocusPress;
    if (bForceViewportMouseBlock && !bAllowViewportFocusPress)
    {
        ClearViewportFocus();
    }
}

void FInputRouter::ClearViewportFocus()
{
    FocusedViewport = nullptr;
    CapturedViewport = nullptr;
    HoveredViewport = nullptr;
    DeactivateAllMouseControl();
    FCursorControl::Clear();
}

void FInputRouter::ForceViewportFocus(FViewport* InViewport)
{
    if (!InViewport)
    {
        ClearViewportFocus();
        return;
    }

    FocusedViewport = InViewport;
    CapturedViewport = nullptr;
    HoveredViewport = InViewport;
    bBlockViewportMouseUntilRelease = false;
}

void FInputRouter::ClearTargets()
{
    Targets.clear();
    HoveredViewport = nullptr;
}

void FInputRouter::RegisterTarget(
    FViewport* InViewport,
    FViewportClient* InClient,
    EInteractionDomain InDomain,
    FRectProvider InRectProvider,
    FWorldResolver InWorldResolver)
{
    if (!InViewport || !InClient || !InRectProvider)
    {
        return;
    }

    FTargetEntry Entry;
    Entry.Viewport = InViewport;
    Entry.Client = InClient;
    Entry.Domain = InDomain;
    Entry.RectProvider = std::move(InRectProvider);
    Entry.WorldResolver = std::move(InWorldResolver);
    Targets.push_back(std::move(Entry));
}

bool FInputRouter::Tick(float DeltaTime, FViewportInputContext& OutContext, FInteractionBinding& OutBinding)
{
    const FInputSystemSnapshot InputSnapshot = InputSystem::Get().TickAndMakeSnapshot();
    FPointerButtonsState PointerButtonsState = ComputePointerButtonsState(InputSnapshot);
    if (!PointerButtonsState.bAnyDown)
    {
        bBlockViewportMouseUntilRelease = false;
    }

    if (!EnsureRoutingEnvironmentReady())
    {
        return false;
    }

    POINT MouseScreenPos = InputSnapshot.MousePos;
    POINT MouseClientPos = MouseScreenPos;
    ScreenToClient(OwnerWindow, &MouseClientPos);

    FTargetEntry* HoveredEntry = nullptr;
    FRect HoveredRect = {};
    TryFindHoveredTarget(MouseClientPos, HoveredEntry, HoveredRect);

    HoveredViewport = HoveredEntry ? HoveredEntry->Viewport : nullptr;

    const bool bMouseCanBelongToViewport = HoveredEntry != nullptr || CapturedViewport != nullptr || bRelativeMouseModeActive;
    if (bImGuiCaptureMouse && PointerButtonsState.bAnyDown && !bMouseCanBelongToViewport)
    {
        bBlockViewportMouseUntilRelease = true;
    }

    const bool bViewportFocusPress = bAllowViewportFocusPress && HoveredEntry != nullptr && PointerButtonsState.bAnyPressed;
    const bool bHardBlockMouse = (bForceViewportMouseBlock && !bViewportFocusPress) || bBlockViewportMouseUntilRelease;
    if (bHardBlockMouse && bRelativeMouseModeActive)
    {
        DeactivateRelativeMouseMode();
    }
    if (bHardBlockMouse && bAbsoluteMouseClipActive)
    {
        DeactivateAbsoluteMouseClip();
    }

    UpdatePointerTrackingState(HoveredEntry, PointerButtonsState.bAnyPressed, PointerButtonsState.bAnyDown, bHardBlockMouse);

    FRect TargetRect = {};
    FTargetEntry* TargetEntry = ResolveDispatchTarget(HoveredEntry, HoveredRect, PointerButtonsState.bAnyDown, TargetRect);
    if (!TargetEntry)
    {
        DeactivateAllMouseControl();
        FCursorControl::Clear();
        return false;
    }
    PopulateDispatchContext(InputSnapshot, DeltaTime, MouseClientPos, TargetEntry, TargetRect, OutContext, OutBinding);

    if (!bHardBlockMouse)
    {
        UpdateRelativeMouseModeState(TargetEntry, TargetRect, InputSnapshot, MouseScreenPos, OutContext);
    }
    else
    {
        OutContext.bRelativeMouseMode = false;
        OutContext.bImGuiCapturedMouse = bImGuiCaptureMouse;
    }

    if (!OutContext.bRelativeMouseMode && !bHardBlockMouse)
    {
        UpdateAbsoluteClipState(TargetEntry, OutContext);
    }

    FinalizeAndDispatchInput(TargetEntry, TargetRect, InputSnapshot, MouseScreenPos, bHardBlockMouse, OutContext);
    if (!OutContext.bRelativeMouseMode && !bAbsoluteMouseClipActive && !PointerButtonsState.bAnyDown && FCursorControl::IsCursorHidden())
    {
        FCursorControl::Clear();
    }
    return true;
}

bool FInputRouter::IsPointInRect(const POINT& Point, const FRect& Rect)
{
    return Point.x >= Rect.X
        && Point.x < (Rect.X + Rect.Width)
        && Point.y >= Rect.Y
        && Point.y < (Rect.Y + Rect.Height);
}

void FInputRouter::ResetTrackingState()
{
    HoveredViewport = nullptr;
    FocusedViewport = nullptr;
    CapturedViewport = nullptr;
    bBlockViewportMouseUntilRelease = false;
}

void FInputRouter::PopulateDispatchContext(
    const FInputSystemSnapshot& InputSnapshot,
    float DeltaTime,
    const POINT& MouseClientPos,
    FTargetEntry* TargetEntry,
    const FRect& TargetRect,
    FViewportInputContext& OutContext,
    FInteractionBinding& OutBinding)
{
    OutContext = {};
    OutBinding = {};
    const bool bBlockKeyboardForViewport = bImGuiCaptureKeyboard;

    FInputFrame Frame = BuildFrameFromSnapshot(InputSnapshot, ++InputFrameCounter, OwnerWindow);
    if (bBlockKeyboardForViewport)
    {
        for (int32 VK = 0; VK < 256; ++VK)
        {
            if (!IsMouseButtonKey(VK))
            {
                Frame.KeyDown[VK] = false;
            }
        }
    }

    OutContext.Frame = Frame;
    OutContext.DeltaSeconds = (DeltaTime > 0.0f) ? DeltaTime : 0.0f;
    OutContext.TargetViewport = TargetEntry->Viewport;
    OutContext.TargetClient = TargetEntry->Client;
    OutContext.Domain = TargetEntry->Domain;
    OutContext.TargetWorld = TargetEntry->WorldResolver ? TargetEntry->WorldResolver() : nullptr;
    OutContext.MouseClientPos = MouseClientPos;
    OutContext.MouseLocalPos =
    {
        MouseClientPos.x - static_cast<LONG>(TargetRect.X),
        MouseClientPos.y - static_cast<LONG>(TargetRect.Y)
    };
    OutContext.MouseLocalDelta = Frame.MouseDelta;
    OutContext.bHovered = (HoveredViewport == TargetEntry->Viewport);
    OutContext.bFocused = (FocusedViewport == TargetEntry->Viewport);
    OutContext.bCaptured = (CapturedViewport == TargetEntry->Viewport);
    const bool bViewportPointerTarget =
        OutContext.bHovered ||
        OutContext.bCaptured ||
        OutContext.bRelativeMouseMode;
    OutContext.bImGuiCapturedMouse = bImGuiCaptureMouse && !bViewportPointerTarget;
    OutContext.bImGuiCapturedKeyboard = bImGuiCaptureKeyboard;
    OutContext.bRelativeMouseMode = bRelativeMouseModeActive && (RelativeMouseModeViewport == TargetEntry->Viewport);

    OutBinding.ReceiverVC = TargetEntry->Client;
    OutBinding.TargetWorld = OutContext.TargetWorld;
    OutBinding.Domain = OutContext.Domain;
}

bool FInputRouter::EnsureRoutingEnvironmentReady()
{
    if (!OwnerWindow || Targets.empty())
    {
        ClearRoutingStateAndMouseControl();
        return false;
    }

    if (GetForegroundWindow() != OwnerWindow)
    {
        ClearRoutingStateAndMouseControl();
        return false;
    }

    return true;
}

void FInputRouter::ClearRoutingStateAndMouseControl()
{
    ResetTrackingState();
    DeactivateAllMouseControl();
    FCursorControl::Clear();
}

void FInputRouter::UpdateRelativeMouseModeState(
    FTargetEntry* TargetEntry,
    const FRect& TargetRect,
    const FInputSystemSnapshot& InputSnapshot,
    const POINT& MouseScreenPos,
    FViewportInputContext& InOutContext)
{
    POINT RestoreScreenPos = InOutContext.Frame.MouseScreenPos;
    const bool bWantsRelativeMouseMode = TargetEntry->Client->WantsRelativeMouseMode(InOutContext, RestoreScreenPos);
    const bool bRelativeViewportMismatch = bRelativeMouseModeActive && (RelativeMouseModeViewport != TargetEntry->Viewport);
    bool bActivatedRelativeMouseModeThisFrame = false;
    if (bRelativeMouseModeActive && (!bWantsRelativeMouseMode || bRelativeViewportMismatch))
    {
        DeactivateRelativeMouseMode();
        InOutContext.bRelativeMouseMode = false;
    }
    if (!bRelativeMouseModeActive && bWantsRelativeMouseMode)
    {
        const RECT ClipRect = GetTargetRectScreenRect(TargetRect);
        ActivateRelativeMouseMode(TargetEntry->Viewport, RestoreScreenPos, ClipRect);
        InOutContext.bRelativeMouseMode = true;
        bActivatedRelativeMouseModeThisFrame = true;
    }
    else if (bRelativeMouseModeActive)
    {
        InOutContext.bRelativeMouseMode = (RelativeMouseModeViewport == TargetEntry->Viewport);
    }

    if (!InOutContext.bRelativeMouseMode)
    {
        return;
    }

    if (bAbsoluteMouseClipActive)
    {
        DeactivateAbsoluteMouseClip();
    }

    POINT LockScreenPos = GetTargetRectScreenCenter(TargetRect);
    const FCursorControlState CursorState = FCursorControl::GetState();
    if (CursorState.bLockToScreenPos)
    {
        LockScreenPos = CursorState.LockScreenPos;
    }

    InOutContext.Frame.MouseInputMode = EMouseInputMode::Relative;
    if (bActivatedRelativeMouseModeThisFrame)
    {
        InOutContext.Frame.MouseDelta = { 0, 0 };
        InOutContext.MouseLocalDelta = { 0, 0 };
    }
    else if (!InputSnapshot.bUsingRawMouse)
    {
        InOutContext.Frame.MouseDelta.x = MouseScreenPos.x - LockScreenPos.x;
        InOutContext.Frame.MouseDelta.y = MouseScreenPos.y - LockScreenPos.y;
        InOutContext.MouseLocalDelta = InOutContext.Frame.MouseDelta;
    }

    InOutContext.Frame.MouseScreenPos = LockScreenPos;
    POINT LockClientPos = LockScreenPos;
    ScreenToClient(OwnerWindow, &LockClientPos);
    InOutContext.MouseClientPos = LockClientPos;
    InOutContext.MouseLocalPos.x = LockClientPos.x - static_cast<LONG>(TargetRect.X);
    InOutContext.MouseLocalPos.y = LockClientPos.y - static_cast<LONG>(TargetRect.Y);
    FCursorControl::Apply();
}

void FInputRouter::UpdateAbsoluteClipState(FTargetEntry* TargetEntry, const FViewportInputContext& Context)
{
    RECT AbsoluteClipRect = {};
    const bool bWantsAbsoluteClip = TargetEntry->Client->WantsAbsoluteMouseClip(Context, AbsoluteClipRect);
    const bool bAbsoluteClipMismatch =
        bAbsoluteMouseClipActive
        && (AbsoluteMouseClipViewport != TargetEntry->Viewport
            || AbsoluteMouseClipRect.left != AbsoluteClipRect.left
            || AbsoluteMouseClipRect.top != AbsoluteClipRect.top
            || AbsoluteMouseClipRect.right != AbsoluteClipRect.right
            || AbsoluteMouseClipRect.bottom != AbsoluteClipRect.bottom);

    if (bAbsoluteMouseClipActive && (!bWantsAbsoluteClip || bAbsoluteClipMismatch))
    {
        DeactivateAbsoluteMouseClip();
    }
    if (!bAbsoluteMouseClipActive && bWantsAbsoluteClip)
    {
        ActivateAbsoluteMouseClip(TargetEntry->Viewport, AbsoluteClipRect);
    }
}

void FInputRouter::DeactivateAllMouseControl()
{
    if (bRelativeMouseModeActive)
    {
        DeactivateRelativeMouseMode();
    }
    if (bAbsoluteMouseClipActive)
    {
        DeactivateAbsoluteMouseClip();
    }
}

void FInputRouter::ValidateTrackedViewports()
{
    bool bFocusedStillValid = false;
    bool bCapturedStillValid = false;
    for (const FTargetEntry& Entry : Targets)
    {
        bFocusedStillValid |= (FocusedViewport == Entry.Viewport);
        bCapturedStillValid |= (CapturedViewport == Entry.Viewport);
    }

    if (!bFocusedStillValid)
    {
        FocusedViewport = nullptr;
    }
    if (!bCapturedStillValid)
    {
        CapturedViewport = nullptr;
    }
}

void FInputRouter::UpdatePointerTrackingState(FTargetEntry* HoveredEntry, bool& bAnyPointerPressed, bool& bAnyPointerDown, bool bHardBlockMouse)
{
    ValidateTrackedViewports();

    const bool bViewportCanOwnPointer = HoveredEntry != nullptr;
    if (bHardBlockMouse || (bImGuiCaptureMouse && !bViewportCanOwnPointer))
    {
        bAnyPointerPressed = false;
        bAnyPointerDown = false;
        CapturedViewport = nullptr;
    }

    if (bAnyPointerPressed && HoveredEntry)
    {
        FocusedViewport = HoveredEntry->Viewport;
        CapturedViewport = HoveredEntry->Viewport;
    }

    if (!bAnyPointerDown)
    {
        CapturedViewport = nullptr;
    }
}

void FInputRouter::FinalizeAndDispatchInput(
    FTargetEntry* TargetEntry,
    const FRect& TargetRect,
    const FInputSystemSnapshot& InputSnapshot,
    const POINT& MouseScreenPos,
    bool bHardBlockMouse,
    FViewportInputContext& InOutContext)
{
    AppendKeyEvents(InputSnapshot, MouseScreenPos, InOutContext.Frame.MouseDelta, InOutContext.Events);
    AppendWheelEvent(InOutContext.Frame.WheelNotches, MouseScreenPos, InOutContext.Events);
    AppendDragEvents(InputSnapshot, MouseScreenPos, InOutContext.Events);

    const bool bViewportPointerTarget =
        InOutContext.bHovered ||
        InOutContext.bCaptured ||
        InOutContext.bRelativeMouseMode;
    const bool bBlockKeyboardForViewport = InOutContext.bImGuiCapturedKeyboard || !InOutContext.bFocused;
    const bool bBlockMouseForViewport =
        bHardBlockMouse ||
        !InOutContext.bFocused ||
        (bImGuiCaptureMouse && !bViewportPointerTarget);
    ApplyViewportBlockMask(bBlockKeyboardForViewport, bBlockMouseForViewport, InOutContext);

    InOutContext.bConsumed = TargetEntry->Client->ProcessInput(InOutContext);
    if (InOutContext.bRelativeMouseMode)
    {
        POINT LockScreenPos = GetTargetRectScreenCenter(TargetRect);
        const FCursorControlState CursorState = FCursorControl::GetState();
        if (CursorState.bLockToScreenPos)
        {
            LockScreenPos = CursorState.LockScreenPos;
        }
        SetCursorPos(LockScreenPos.x, LockScreenPos.y);
    }
}

bool FInputRouter::TryFindHoveredTarget(const POINT& MouseClientPos, FTargetEntry*& OutHoveredEntry, FRect& OutHoveredRect)
{
    OutHoveredEntry = nullptr;
    OutHoveredRect = {};

    for (FTargetEntry& Entry : Targets)
    {
        FRect Rect = {};
        if (!Entry.RectProvider(Rect))
        {
            continue;
        }

        const FRect HitRect = GetInsetRect(Rect, ViewportInputDeadZonePixels);
        if (HitRect.Width <= 0.0f || HitRect.Height <= 0.0f)
        {
            continue;
        }
        if (IsPointInRect(MouseClientPos, HitRect))
        {
            OutHoveredEntry = &Entry;
            OutHoveredRect = Rect;
            return true;
        }
    }

    return false;
}

FInputRouter::FTargetEntry* FInputRouter::ResolveDispatchTarget(FTargetEntry* HoveredEntry, const FRect& HoveredRect, bool bAnyPointerDown, FRect& OutTargetRect)
{
    FTargetEntry* TargetEntry = nullptr;
    OutTargetRect = {};
    if (CapturedViewport)
    {
        TargetEntry = FindEntryByViewport(CapturedViewport, OutTargetRect);
    }
    if (!TargetEntry && HoveredEntry)
    {
        TargetEntry = HoveredEntry;
        OutTargetRect = HoveredRect;
    }
    if (!TargetEntry && FocusedViewport)
    {
        TargetEntry = FindEntryByViewport(FocusedViewport, OutTargetRect);
    }
    if (!TargetEntry && bRelativeMouseModeActive && RelativeMouseModeViewport)
    {
        TargetEntry = FindEntryByViewport(RelativeMouseModeViewport, OutTargetRect);
        if (TargetEntry)
        {
            FocusedViewport = RelativeMouseModeViewport;
            if (bAnyPointerDown)
            {
                CapturedViewport = RelativeMouseModeViewport;
            }
        }
    }
    return TargetEntry;
}

FInputRouter::FTargetEntry* FInputRouter::FindEntryByViewport(FViewport* InViewport, FRect& OutRect)
{
    for (FTargetEntry& Entry : Targets)
    {
        if (Entry.Viewport != InViewport)
        {
            continue;
        }
        if (Entry.RectProvider(OutRect))
        {
            return &Entry;
        }
        return nullptr;
    }
    return nullptr;
}

POINT FInputRouter::GetTargetRectScreenCenter(const FRect& TargetRect) const
{
    POINT CenterClientPos =
    {
        static_cast<LONG>(TargetRect.X + TargetRect.Width * 0.5f),
        static_cast<LONG>(TargetRect.Y + TargetRect.Height * 0.5f)
    };
    if (OwnerWindow)
    {
        ClientToScreen(OwnerWindow, &CenterClientPos);
    }
    return CenterClientPos;
}

RECT FInputRouter::GetTargetRectScreenRect(const FRect& TargetRect) const
{
    POINT TopLeft =
    {
        static_cast<LONG>(TargetRect.X),
        static_cast<LONG>(TargetRect.Y)
    };
    POINT BottomRight =
    {
        static_cast<LONG>(TargetRect.X + TargetRect.Width),
        static_cast<LONG>(TargetRect.Y + TargetRect.Height)
    };

    if (OwnerWindow)
    {
        ClientToScreen(OwnerWindow, &TopLeft);
        ClientToScreen(OwnerWindow, &BottomRight);
    }

    RECT ClipRect = {};
    ClipRect.left = TopLeft.x;
    ClipRect.top = TopLeft.y;
    ClipRect.right = BottomRight.x;
    ClipRect.bottom = BottomRight.y;
    return ClipRect;
}

void FInputRouter::ActivateRelativeMouseMode(FViewport* InViewport, const POINT& RestoreScreenPos, const RECT& ClipScreenRect)
{
    RelativeMouseModeViewport = InViewport;
    RelativeMouseRestorePos = RestoreScreenPos;
    bRelativeMouseModeActive = true;
    InputSystem::Get().SetUseRawMouse(true);

    FCursorControlState CursorState{};
    CursorState.OwnerWindow = OwnerWindow;
    CursorState.bHideInClient = true;
    CursorState.bLockToScreenPos = true;
    CursorState.LockScreenPos = ClampPointToRect(RestoreScreenPos, ClipScreenRect);
    FCursorControl::SetState(CursorState);

    if (OwnerWindow)
    {
        SetCapture(OwnerWindow);
    }
}

void FInputRouter::DeactivateRelativeMouseMode()
{
    bRelativeMouseModeActive = false;
    RelativeMouseModeViewport = nullptr;
    InputSystem::Get().SetUseRawMouse(false);
    FCursorControl::Clear();

    if (OwnerWindow && GetCapture() == OwnerWindow)
    {
        ReleaseCapture();
    }
    SetCursorPos(RelativeMouseRestorePos.x, RelativeMouseRestorePos.y);
}

void FInputRouter::ActivateAbsoluteMouseClip(FViewport* InViewport, const RECT& ClipScreenRect)
{
    AbsoluteMouseClipViewport = InViewport;
    AbsoluteMouseClipRect = ClipScreenRect;
    bAbsoluteMouseClipActive = true;
    ::ClipCursor(&AbsoluteMouseClipRect);
}

void FInputRouter::DeactivateAbsoluteMouseClip()
{
    bAbsoluteMouseClipActive = false;
    AbsoluteMouseClipViewport = nullptr;
    AbsoluteMouseClipRect = { 0, 0, 0, 0 };
    ::ClipCursor(nullptr);
}
