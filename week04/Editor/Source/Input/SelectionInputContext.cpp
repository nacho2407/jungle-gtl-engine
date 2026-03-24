#include "SelectionInputContext.h"

using Engine::ApplicationCore::EInputEventType;
using Engine::ApplicationCore::EKey;

FSelectionInputContext::FSelectionInputContext(FViewportSelectionController* InSelectionController)
    : SelectionController(InSelectionController)
{
}

bool FSelectionInputContext::HandleEvent(const Engine::ApplicationCore::FInputEvent& Event,
                                         const Engine::ApplicationCore::FInputState& State)
{
    if (SelectionController == nullptr)
    {
        return false;
    }

    switch (Event.Type)
    {
    case EInputEventType::MouseButtonDown:
        if (Event.Key == EKey::MouseLeft)
        {
            bRectSelection = IsRectSelectionModifier(State);

            if (State.Modifiers.bAltDown && !bRectSelection)
            {
                return false;
            }

            bLeftMouseDown = true;
            bDragging = false;
            PressMouseX = Event.MouseX;
            PressMouseY = Event.MouseY;

            // 클릭/드래그 의도를 MouseUp/Move에서 확정하기 위해 Down은 선소비하지 않습니다.
            return false;
        }
        break;
    case EInputEventType::MouseMove:
        if (SelectionController->IsDraggingSelection())
        {
            SelectionController->UpdateSelection(Event.MouseX, Event.MouseY);
            return true;
        }

        if (bLeftMouseDown)
        {
            const int32 DeltaX = Event.MouseX - PressMouseX;
            const int32 DeltaY = Event.MouseY - PressMouseY;
            const int32 DistanceSq = DeltaX * DeltaX + DeltaY * DeltaY;

            if (!bDragging && DistanceSq >= DragThreshold * DragThreshold)
            {
                bDragging = true;

                if (bRectSelection)
                {
                    SelectionController->BeginSelection(PressMouseX, PressMouseY,
                                                        ResolveSelectionMode(State));
                }
            }

            if (bDragging && bRectSelection)
            {
                SelectionController->UpdateSelection(Event.MouseX, Event.MouseY);
                return true;
            }
        }
        break;
    case EInputEventType::MouseButtonUp:
        if (Event.Key == EKey::MouseLeft && SelectionController->IsDraggingSelection())
        {
            SelectionController->EndSelection(Event.MouseX, Event.MouseY);

            bLeftMouseDown = false;
            bDragging = false;
            bRectSelection = false;
            return true;
        }

        if (Event.Key == EKey::MouseLeft)
        {
            const bool bWasDragging = bDragging;

            bLeftMouseDown = false;
            bDragging = false;
            bRectSelection = false;

            if (bWasDragging)
            {
                return false;
            }

            if (State.Modifiers.bAltDown)
            {
                return false;
            }

            SelectionController->ClickSelect(Event.MouseX, Event.MouseY,
                                             ResolveSelectionMode(State));

            return true;
        }
        break;
    default:
        break;
    }

    return false;
}

void FSelectionInputContext::Tick(const Engine::ApplicationCore::FInputState& State)
{
    if (SelectionController == nullptr)
    {
        return;
    }
}

ESelectionMode FSelectionInputContext::ResolveSelectionMode(
    const Engine::ApplicationCore::FInputState& State) const
{
    if (IsRectSelectionModifier(State))
    {
        return State.Modifiers.bShiftDown ? ESelectionMode::Add : ESelectionMode::Replace;
    }

    if (State.Modifiers.bShiftDown && !State.Modifiers.bCtrlDown)
    {
        return ESelectionMode::Add;
    }

    if (State.Modifiers.bCtrlDown && !State.Modifiers.bShiftDown)
    {
        return ESelectionMode::Toggle;
    }

    if (State.Modifiers.bCtrlDown && State.Modifiers.bShiftDown)
    {
        return ESelectionMode::Toggle;
    }

    return ESelectionMode::Replace;
}

bool FSelectionInputContext::IsRectSelectionModifier(
    const Engine::ApplicationCore::FInputState& State) const
{
    return State.Modifiers.bCtrlDown && State.Modifiers.bAltDown;
}