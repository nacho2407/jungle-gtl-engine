#include "ViewportInputRouter.h"

#include "Viewport/Viewport.h"
#include "Viewport/ViewportClient.h"

void FViewportInputRouter::ClearTargets()
{
    Targets.clear();
}

void FViewportInputRouter::RegisterTarget(
    FViewport* InViewport,
    FViewportClient* InClient,
    FRectProvider InRectProvider)
{
    if (!InViewport || !InClient || !InRectProvider)
    {
        return;
    }

    FTargetEntry Entry{};
    Entry.Viewport = InViewport;
    Entry.Client = InClient;
    Entry.RectProvider = InRectProvider;

    Targets.push_back(Entry);
}

void FViewportInputRouter::SetKeyTargetViewport(FViewport* InViewport)
{
    if (KeyTargetViewport == InViewport)
    {
        return;
    }

    FRect PreviousRect{};
    if (FTargetEntry* PreviousTarget = FindTargetByViewport(KeyTargetViewport, PreviousRect))
    {
        if (PreviousTarget->Client)
        {
            PreviousTarget->Client->ResetInputState();
        }
    }

    KeyTargetViewport = InViewport;
    ResetKeyRepeatState();
}

void FViewportInputRouter::Tick(const FInputSnapshot& Input, float DeltaTime)
{
    POINT ClientPos = ScreenToClientPoint(Input.MouseScreenPos);

    FRect HoveredRect{};
    FTargetEntry* Hovered = FindHoveredTarget(ClientPos, HoveredRect);
    HoveredViewport = Hovered ? Hovered->Viewport : nullptr;

    FRect PointerTargetRect{};
    FTargetEntry* PointerTarget = ResolvePointerTarget(ClientPos, PointerTargetRect);

    if (PointerTarget)
    {
        DispatchPointerEvents(PointerTarget, PointerTargetRect, Input);
        DispatchAxisEvents(PointerTarget, Input, DeltaTime);
    }

    FRect KeyTargetRect{};
    FTargetEntry* KeyTarget = ResolveKeyTarget(KeyTargetRect);

    if (!KeyTarget)
    {
        ResetKeyRepeatState();
        return;
    }

    DispatchKeyEvents(KeyTarget, Input, DeltaTime);
}

/*
 * private helper functions
 */

bool FViewportInputRouter::IsPointInRect(const POINT& Point, const FRect& Rect)
{
    return Point.x >= Rect.X && Point.y >= Rect.Y && Point.x < Rect.X + Rect.Width && Point.y < Rect.Y + Rect.Height;
}

FViewportInputRouter::FTargetEntry* FViewportInputRouter::FindHoveredTarget(const POINT& ClientPos, FRect& OutRect)
{
    for (FTargetEntry& Entry : Targets)
    {
        FRect Rect{};
        if (!Entry.RectProvider(Rect))
        {
            continue;
        }

        if (Rect.Width <= 0.0f || Rect.Height <= 0.0f)
        {
            continue;
        }

        if (IsPointInRect(ClientPos, Rect))
        {
            OutRect = Rect;
            return &Entry;
        }
    }

    return nullptr;
}

FViewportInputRouter::FTargetEntry* FViewportInputRouter::FindTargetByViewport(FViewport* InViewport, FRect& OutRect)
{
    if (!InViewport)
    {
        return nullptr;
    }

    for (FTargetEntry& Entry : Targets)
    {
        if (Entry.Viewport != InViewport)
        {
            continue;
        }

        FRect Rect{};
        if (!Entry.RectProvider(Rect))
        {
            return nullptr;
        }

        OutRect = Rect;
        return &Entry;
    }

    return nullptr;
}

FViewportInputRouter::FTargetEntry* FViewportInputRouter::ResolvePointerTarget(const POINT& ClientPos, FRect& OutRect)
{
    if (CapturedViewport)
    {
        if (FTargetEntry* Captured = FindTargetByViewport(CapturedViewport, OutRect))
        {
            return Captured;
        }

        CapturedViewport = nullptr;
    }

    return FindHoveredTarget(ClientPos, OutRect);
}

FViewportInputRouter::FTargetEntry* FViewportInputRouter::ResolveKeyTarget(FRect& OutRect)
{
    if (KeyTargetViewport)
    {
        if (FTargetEntry* KeyTarget = FindTargetByViewport(KeyTargetViewport, OutRect))
        {
            return KeyTarget;
        }

        SetKeyTargetViewport(nullptr);
    }

    return nullptr;
}

void FViewportInputRouter::DispatchPointerEvents(FTargetEntry* Target, const FRect& TargetRect, const FInputSnapshot& Input)
{
    if (!Target || !Target->Client)
    {
        return;
    }

    if (GuiCaptureState.bMouse && !CapturedViewport)
    {
        return;
    }

    POINT ClientPos = ScreenToClientPoint(Input.MouseScreenPos);
    POINT LocalPos = ClientToLocalPoint(ClientPos, TargetRect);

    auto MakePointerEvent = [&](EPointerButton Button, EPointerEventType Type)
    {
        FViewportPointerEvent Event{};
        Event.Button = Button;
        Event.Type = Type;
        Event.ScreenPos = Input.MouseScreenPos;
        Event.ClientPos = ClientPos;
        Event.LocalPos = LocalPos;
        Event.Modifiers = Input.Modifiers;
        return Event;
    };

	// Pressed
    if (Input.KeyPressed[VK_LBUTTON])
    {
        CapturedViewport = Target->Viewport;
        SetKeyTargetViewport(Target->Viewport);

        Target->Client->InputPointer(MakePointerEvent(EPointerButton::Left, EPointerEventType::Pressed));
    }

    if (Input.KeyPressed[VK_RBUTTON])
    {
        CapturedViewport = Target->Viewport;
        SetKeyTargetViewport(Target->Viewport);

        Target->Client->InputPointer(MakePointerEvent(EPointerButton::Right, EPointerEventType::Pressed));
    }

    if (Input.KeyPressed[VK_MBUTTON])
    {
        CapturedViewport = Target->Viewport;
        SetKeyTargetViewport(Target->Viewport);

        Target->Client->InputPointer(MakePointerEvent(EPointerButton::Middle, EPointerEventType::Pressed));
    }

	// Moved
    if (Input.MouseDelta.x != 0 || Input.MouseDelta.y != 0)
    {
        Target->Client->InputPointer(MakePointerEvent(EPointerButton::None, EPointerEventType::Moved));
    }

	// Released
    if (Input.KeyReleased[VK_LBUTTON])
    {
        Target->Client->InputPointer(MakePointerEvent(EPointerButton::Left, EPointerEventType::Released));
        CapturedViewport = nullptr;
    }

    if (Input.KeyReleased[VK_RBUTTON])
    {
        Target->Client->InputPointer(MakePointerEvent(EPointerButton::Right, EPointerEventType::Released));
        CapturedViewport = nullptr;
    }

    if (Input.KeyReleased[VK_MBUTTON])
    {
        Target->Client->InputPointer(MakePointerEvent(EPointerButton::Middle, EPointerEventType::Released));
        CapturedViewport = nullptr;
    }
}

void FViewportInputRouter::DispatchAxisEvents(FTargetEntry* Target, const FInputSnapshot& Input, float DeltaTime)
{
    if (!Target || !Target->Client)
    {
        return;
    }

    if (GuiCaptureState.bMouse && !CapturedViewport)
    {
        return;
    }

    auto DispatchAxis = [&](EInputAxis Axis, float Value, int32 ControllerId = 0)
    {
        if (Value == 0.0f)
        {
            return;
        }

        FViewportAxisEvent Event{};
        Event.Axis = Axis;
        Event.Value = Value;
        Event.DeltaTime = DeltaTime;
        // Event.ControllerId = ControllerId; TODO: Milestone 3에서 Gamepad 입력이 추가되면, ControllerId도 전달할 예정
        Event.Modifiers = Input.Modifiers;

        Target->Client->InputAxis(Event);
    };

    DispatchAxis(EInputAxis::MouseX, static_cast<float>(Input.MouseDelta.x));
    DispatchAxis(EInputAxis::MouseY, static_cast<float>(Input.MouseDelta.y));
    DispatchAxis(EInputAxis::MouseWheel, Input.WheelNotches);

    // TODO: Milestone 3에서 Gamepad 입력이 추가되면, 이 부분도 구현할 예정
    // DispatchAxis(EInputAxis::GamepadLeftX, Input.Gamepads[0].LeftX, 0);
    // DispatchAxis(EInputAxis::GamepadLeftY, Input.Gamepads[0].LeftY, 0);
}

inline static bool IsMouseButtonVK(int32 VK)
{
    return VK == VK_LBUTTON || VK == VK_RBUTTON || VK == VK_MBUTTON || VK == VK_XBUTTON1 || VK == VK_XBUTTON2;
}

void FViewportInputRouter::DispatchKeyEvents(FTargetEntry* Target, const FInputSnapshot& Input, float DeltaTime)
{
    /*
     * note: DispatchKeyEvents는 모든 키에 대한 Repeat 이벤트를 라우팅하지만, 실제로 모든 키에 Repeat 이벤트가 필요한 것은 아님!
     *       때문에 이벤트를 받는 Tool 측에서 상황에 따라 필요한 키에 대해서만 Repeat 이벤트를 처리하는 형태로 구현해야 함
     */

    if (!Target || !Target->Client)
    {
        return;
    }

    if (GuiCaptureState.bKeyboard)
    {
        // GUI가 키보드 입력을 캡처하는 경우, 모든 키에 대해 Key Repeat 상태를 초기화
        // GUI 내부의 Key Repeat은 GUI 시스템(우리 프로젝트의 경우 ImGui)이 자체적으로 처리하기를 기대
        Target->Client->ResetInputState();
        ResetKeyRepeatState();
        return;
    }

    auto DispatchKey = [&](int32 VK, EKeyEventType Type)
    {
        FViewportKeyEvent Event{};
        Event.Key = VK;
        Event.Type = Type;
        Event.Modifiers = Input.Modifiers;
        Target->Client->InputKey(Event);
    };

    for (int32 VK = 0; VK < 256; ++VK)
    {
        // 마우스 버튼은 Pointer 이벤트로만 처리해야 함
        if (IsMouseButtonVK(VK))
        {
            continue;
        }

        if (Input.KeyPressed[VK])
        {
            DispatchKey(VK, EKeyEventType::Pressed);

            KeyRepeatElapsed[VK] = 0.0f;
            bKeyRepeatActive[VK] = false;
            continue;
        }

        if (Input.KeyReleased[VK])
        {
            DispatchKey(VK, EKeyEventType::Released);

            KeyRepeatElapsed[VK] = 0.0f;
            bKeyRepeatActive[VK] = false;
            continue;
        }

        // 새로 Pressed나 Released된 상태가 아님에도 Down 상태인 키에 대해서는 Repeat 처리
        if (Input.KeyDown[VK])
        {
            KeyRepeatElapsed[VK] += DeltaTime;

            if (!bKeyRepeatActive[VK])
            {
                // Initial Delay가 지나면 Repeat 이벤트 시작
                if (KeyRepeatElapsed[VK] >= InitialKeyRepeatDelay)
                {
                    DispatchKey(VK, EKeyEventType::Repeat);

                    bKeyRepeatActive[VK] = true;
                    KeyRepeatElapsed[VK] = 0.0f;
                }
            }
            else
            {
                if (KeyRepeatElapsed[VK] >= KeyRepeatInterval)
                {
                    DispatchKey(VK, EKeyEventType::Repeat);

                    // 0으로 초기화하지 않는 이유? DeltaTime이 큰 프레임에서 여러 번 Repeat 이벤트가 발생해야 할 수 있기 때문!
                    KeyRepeatElapsed[VK] -= KeyRepeatInterval;
                }
            }
        }
        else
        {
            KeyRepeatElapsed[VK] = 0.0f;
            bKeyRepeatActive[VK] = false;
        }
    }
}

POINT FViewportInputRouter::ScreenToClientPoint(POINT ScreenPos) const
{
    POINT ClientPos = ScreenPos;
    if (OwnerWindow)
    {
        ScreenToClient(OwnerWindow, &ClientPos);
    }
    return ClientPos;
}

POINT FViewportInputRouter::ClientToLocalPoint(const POINT& ClientPos, const FRect& TargetRect) const
{
    POINT Local{};
    Local.x = static_cast<LONG>(ClientPos.x - TargetRect.X);
    Local.y = static_cast<LONG>(ClientPos.y - TargetRect.Y);
    return Local;
}

void FViewportInputRouter::ResetKeyRepeatState()
{
    for (int32 VK = 0; VK < 256; ++VK)
    {
        KeyRepeatElapsed[VK] = 0.0f;
        bKeyRepeatActive[VK] = false;
    }
}
