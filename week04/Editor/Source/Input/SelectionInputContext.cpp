#include "SelectionInputContext.h"

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
        //  Initial Click
    case Engine::ApplicationCore::EInputEventType::MouseButtonDown :
        if (Event.Key == Engine::ApplicationCore::EKey::MouseLeft)
        {
            bLeftMouseDown = true;
            bDragging = false;
            PressMouseX = Event.MouseX;
            PressMouseY = Event.MouseY;
            return true;
        }
        break;
    case Engine::ApplicationCore::EInputEventType::MouseMove :
        if (bLeftMouseDown)
        {
            const int32 DeltaX = Event.MouseX - PressMouseX;
            const int32 DeltaY = Event.MouseY - PressMouseY;
            const int32 DistanceSq = DeltaX * DeltaX + DeltaY * DeltaY;
            
            if (!bDragging && DistanceSq >= DragThreshold * DragThreshold)
            {
                bDragging = true;
                SelectionController->BeginSelection(PressMouseX,PressMouseY,ResolveSelectionMode(State));
            }
            
            if (bDragging)
            {
                SelectionController->UpdateSelection(Event.MouseX, Event.MouseY);
                return true;
            }
        }
        break;
        case Engine::ApplicationCore::EInputEventType::MouseButtonUp :
        if (Event.Key == Engine::ApplicationCore::EKey::MouseLeft)
        {
            if (bDragging)
            {
                SelectionController->EndSelection(Event.MouseX, Event.MouseY);
            }
            else
            {
                SelectionController->ClickSelect(Event.MouseX, Event.MouseY, ResolveSelectionMode(State));
            }
            
            bLeftMouseDown = false;
            bDragging = false;
            
            return true;
        }
        break;
    default :
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
    if (State.Modifiers.bShiftDown)
    {
        return ESelectionMode::Add;
    }
    if (State.Modifiers.bCtrlDown)
    {
        return ESelectionMode::Toggle;
    }
    
    return ESelectionMode::Replace;
}