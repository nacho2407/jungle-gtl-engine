#pragma once

#include <functional>
#include <windows.h>

#include "Core/CoreMinimal.h"

class FViewport;
class FViewportClient;
class UWorld;

enum class EInteractionDomain : uint8
{
    Editor,
    PIE,
    EditorOnPIE
};

enum class EMouseInputMode : uint8
{
    Absolute,
    Relative
};

enum class EInputEventType : uint8
{
    KeyPressed,
    KeyReleased,
    WheelScrolled,
    PointerDragStarted,
    PointerDragEnded
};

enum class EPointerButton : uint8
{
    None,
    Left,
    Right,
    Middle
};

struct FInputEvent
{
    EInputEventType Type = EInputEventType::KeyPressed;
    int32           Key = 0;
    EPointerButton  PointerButton = EPointerButton::None;
    POINT           MouseScreenPos = { 0, 0 };
    POINT           MouseDelta = { 0, 0 };
    float           WheelNotches = 0.0f;
};

struct FInputModifiers
{
    bool bCtrl = false;
    bool bAlt = false;
    bool bShift = false;
};

struct FInputFrame
{
    uint64          FrameNumber = 0;
    HWND            SourceWindow = nullptr;
    EMouseInputMode MouseInputMode = EMouseInputMode::Absolute;

    POINT MouseScreenPos = { 0, 0 };
    POINT MouseDelta = { 0, 0 };
    float WheelNotches = 0.0f;

    bool  KeyDown[256] = {};
    bool  bLeftDragging = false;
    bool  bMiddleDragging = false;
    bool  bRightDragging = false;
    POINT LeftDragVector = { 0, 0 };
    POINT MiddleDragVector = { 0, 0 };
    POINT RightDragVector = { 0, 0 };

    bool IsDown(int32 VK) const { return VK >= 0 && VK < 256 && KeyDown[VK]; }
    bool IsCtrlDown() const { return IsDown(VK_CONTROL); }
    bool IsAltDown() const { return IsDown(VK_MENU); }
    bool IsShiftDown() const { return IsDown(VK_SHIFT); }

    FInputModifiers GetModifiers() const
    {
        FInputModifiers Modifiers{};
        Modifiers.bCtrl = IsCtrlDown();
        Modifiers.bAlt = IsAltDown();
        Modifiers.bShift = IsShiftDown();
        return Modifiers;
    }
};

struct FInputChord
{
    int32 Key = 0;
    bool  bCtrl = false;
    bool  bAlt = false;
    bool  bShift = false;

    bool MatchesState(const FInputFrame& Frame) const
    {
        return Frame.IsCtrlDown() == bCtrl
            && Frame.IsAltDown() == bAlt
            && Frame.IsShiftDown() == bShift;
    }
};

struct FPointerGesture
{
    EPointerButton Button = EPointerButton::None;
    bool           bStarted = false;
    bool           bActive = false;
    bool           bEnded = false;
    POINT          TotalDelta = { 0, 0 };
    POINT          FrameDelta = { 0, 0 };
};

struct FInteractionBinding
{
    FViewportClient*    ReceiverVC = nullptr;
    UWorld*             TargetWorld = nullptr;
    EInteractionDomain  Domain = EInteractionDomain::Editor;
};

struct FViewportInputContext
{
    FInputFrame        Frame;
    TArray<FInputEvent> Events;
    float              DeltaSeconds = 0.0f;

    FViewport*         TargetViewport = nullptr;
    FViewportClient*   TargetClient = nullptr;
    UWorld*            TargetWorld = nullptr;
    EInteractionDomain Domain = EInteractionDomain::Editor;

    POINT MouseClientPos = { 0, 0 };
    POINT MouseLocalPos = { 0, 0 };
    POINT MouseLocalDelta = { 0, 0 };

    bool bHovered = false;
    bool bFocused = false;
    bool bCaptured = false;
    bool bImGuiCapturedMouse = false;
    bool bImGuiCapturedKeyboard = false;
    bool bRelativeMouseMode = false;
    bool bConsumed = false;

    bool HasEvent(EInputEventType Type) const
    {
        for (const FInputEvent& Event : Events)
        {
            if (Event.Type == Type)
            {
                return true;
            }
        }
        return false;
    }

    bool WasPressed(int32 VK) const
    {
        for (const FInputEvent& Event : Events)
        {
            if (Event.Type == EInputEventType::KeyPressed && Event.Key == VK)
            {
                return true;
            }
        }
        return false;
    }

    bool WasReleased(int32 VK) const
    {
        for (const FInputEvent& Event : Events)
        {
            if (Event.Type == EInputEventType::KeyReleased && Event.Key == VK)
            {
                return true;
            }
        }
        return false;
    }

    bool MatchesChordPressed(const FInputChord& Chord) const
    {
        return WasPressed(Chord.Key) && Chord.MatchesState(Frame);
    }

    bool MatchesChordDown(const FInputChord& Chord) const
    {
        return Frame.IsDown(Chord.Key) && Chord.MatchesState(Frame);
    }

    bool GetPointerGesture(EPointerButton Button, FPointerGesture& OutGesture) const
    {
        OutGesture = {};
        OutGesture.Button = Button;

        switch (Button)
        {
        case EPointerButton::Left:
            OutGesture.bActive = Frame.bLeftDragging;
            OutGesture.TotalDelta = Frame.LeftDragVector;
            break;
        case EPointerButton::Right:
            OutGesture.bActive = Frame.bRightDragging;
            OutGesture.TotalDelta = Frame.RightDragVector;
            break;
        case EPointerButton::Middle:
            OutGesture.bActive = Frame.bMiddleDragging;
            OutGesture.TotalDelta = Frame.MiddleDragVector;
            break;
        default:
            return false;
        }

        OutGesture.FrameDelta = Frame.MouseDelta;
        for (const FInputEvent& Event : Events)
        {
            if (Event.PointerButton != Button)
            {
                continue;
            }
            if (Event.Type == EInputEventType::PointerDragStarted)
            {
                OutGesture.bStarted = true;
            }
            else if (Event.Type == EInputEventType::PointerDragEnded)
            {
                OutGesture.bEnded = true;
            }
        }
        return OutGesture.bStarted || OutGesture.bActive || OutGesture.bEnded;
    }

    bool WasPointerDragStarted(EPointerButton Button) const
    {
        for (const FInputEvent& Event : Events)
        {
            if (Event.PointerButton == Button && Event.Type == EInputEventType::PointerDragStarted)
            {
                return true;
            }
        }
        return false;
    }

    bool WasPointerDragEnded(EPointerButton Button) const
    {
        for (const FInputEvent& Event : Events)
        {
            if (Event.PointerButton == Button && Event.Type == EInputEventType::PointerDragEnded)
            {
                return true;
            }
        }
        return false;
    }
};
