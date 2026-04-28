#include "EditorViewportInputController.h"

#include "EditorGizmoTool.h"
#include "EditorViewportContextMenuTool.h"
#include "EditorNavigationTool.h"
#include "EditorSelectionTool.h"
#include "EditorViewportCommandTool.h"
#include "EditorViewportTool.h"

FEditorViewportInputController::FEditorViewportInputController(FEditorViewportClient* InOwner)
    : Owner(InOwner)
{
	// 순서 중요!!!
    Tools.emplace_back(std::make_unique<FEditorViewportCommandTool>(Owner, this));
    Tools.emplace_back(std::make_unique<FEditorGizmoTool>(Owner, this));
    Tools.emplace_back(std::make_unique<FEditorViewportContextMenuTool>(Owner, this));
    Tools.emplace_back(std::make_unique<FEditorSelectionTool>(Owner, this));
    Tools.emplace_back(std::make_unique<FEditorNavigationTool>(Owner, this));
}

bool FEditorViewportInputController::InputKey(const FViewportKeyEvent& Event)
{
    if (Event.Key < 0 || Event.Key >= 256)
    {
        return false;
    }

    CurrentInput.Modifiers = Event.Modifiers;

    switch (Event.Type)
    {
    case EKeyEventType::Pressed:
        CurrentInput.KeyPressed[Event.Key] = true;
        CurrentInput.KeyDown[Event.Key] = true;
        break;

    case EKeyEventType::Released:
        CurrentInput.KeyReleased[Event.Key] = true;
        CurrentInput.KeyDown[Event.Key] = false;
        break;

    case EKeyEventType::Repeat:
        CurrentInput.KeyRepeated[Event.Key] = true;
        CurrentInput.KeyDown[Event.Key] = true;
        break;
    }

    return false;
}

bool FEditorViewportInputController::InputAxis(const FViewportAxisEvent& Event)
{
    CurrentInput.Modifiers = Event.Modifiers;

    switch (Event.Axis)
    {
    case EInputAxis::MouseX:
        CurrentInput.MouseAxisDelta.x += static_cast<LONG>(Event.Value);
        break;

    case EInputAxis::MouseY:
        CurrentInput.MouseAxisDelta.y += static_cast<LONG>(Event.Value);
        break;

    case EInputAxis::MouseWheel:
        CurrentInput.MouseWheelNotches += Event.Value;
        break;

    default:
        break;
    }

    return false;
}

bool FEditorViewportInputController::InputPointer(const FViewportPointerEvent& Event)
{
    CurrentInput.Modifiers = Event.Modifiers;

    CurrentInput.MouseLocalPos = Event.LocalPos;
    CurrentInput.MouseClientPos = Event.ClientPos;
    CurrentInput.MouseScreenPos = Event.ScreenPos;

    switch (Event.Button)
    {
    case EPointerButton::Left:
        if (Event.Type == EPointerEventType::Pressed)
        {
            CurrentInput.bLeftPressed = true;
            CurrentInput.bLeftDown = true;
        }
        else if (Event.Type == EPointerEventType::Released)
        {
            CurrentInput.bLeftReleased = true;
            CurrentInput.bLeftDown = false;
        }
        break;

    case EPointerButton::Right:
        if (Event.Type == EPointerEventType::Pressed)
        {
            CurrentInput.bRightPressed = true;
            CurrentInput.bRightDown = true;
        }
        else if (Event.Type == EPointerEventType::Released)
        {
            CurrentInput.bRightReleased = true;
            CurrentInput.bRightDown = false;
        }
        break;

    case EPointerButton::Middle:
        if (Event.Type == EPointerEventType::Pressed)
        {
            CurrentInput.bMiddlePressed = true;
            CurrentInput.bMiddleDown = true;
        }
        else if (Event.Type == EPointerEventType::Released)
        {
            CurrentInput.bMiddleReleased = true;
            CurrentInput.bMiddleDown = false;
        }
        break;

    case EPointerButton::X1:
        if (Event.Type == EPointerEventType::Pressed)
        {
            CurrentInput.bX1Pressed = true;
            CurrentInput.bX1Down = true;
        }
        else if (Event.Type == EPointerEventType::Released)
        {
            CurrentInput.bX1Released = true;
            CurrentInput.bX1Down = false;
        }
        break;

    case EPointerButton::X2:
        if (Event.Type == EPointerEventType::Pressed)
        {
            CurrentInput.bX2Pressed = true;
            CurrentInput.bX2Down = true;
        }
        else if (Event.Type == EPointerEventType::Released)
        {
            CurrentInput.bX2Released = true;
            CurrentInput.bX2Down = false;
        }
        break;

    case EPointerButton::None:
    default:
        break;
    }

    return false;
}

void FEditorViewportInputController::BeginInputFrame()
{
    for (int32 VK = 0; VK < 256; ++VK)
    {
        CurrentInput.KeyPressed[VK] = false;
        CurrentInput.KeyReleased[VK] = false;
        CurrentInput.KeyRepeated[VK] = false;
    }

    CurrentInput.MouseAxisDelta = { 0, 0 };
    CurrentInput.MouseWheelNotches = 0.0f;

    CurrentInput.bLeftPressed = false;
    CurrentInput.bLeftReleased = false;

    CurrentInput.bRightPressed = false;
    CurrentInput.bRightReleased = false;

    CurrentInput.bMiddlePressed = false;
    CurrentInput.bMiddleReleased = false;

    CurrentInput.bX1Pressed = false;
    CurrentInput.bX1Released = false;

    CurrentInput.bX2Pressed = false;
    CurrentInput.bX2Released = false;
}

void FEditorViewportInputController::ResetInputState()
{
    for (int32 VK = 0; VK < 256; ++VK)
    {
        CurrentInput.KeyDown[VK] = false;
        CurrentInput.KeyPressed[VK] = false;
        CurrentInput.KeyReleased[VK] = false;
        CurrentInput.KeyRepeated[VK] = false;
    }

    CurrentInput.MouseAxisDelta = { 0, 0 };
    CurrentInput.MouseWheelNotches = 0.0f;

    CurrentInput.bLeftPressed = false;
    CurrentInput.bLeftDown = false;
    CurrentInput.bLeftReleased = false;

    CurrentInput.bRightPressed = false;
    CurrentInput.bRightDown = false;
    CurrentInput.bRightReleased = false;

    CurrentInput.bMiddlePressed = false;
    CurrentInput.bMiddleDown = false;
    CurrentInput.bMiddleReleased = false;

    CurrentInput.bX1Pressed = false;
    CurrentInput.bX1Down = false;
    CurrentInput.bX1Released = false;

    CurrentInput.bX2Pressed = false;
    CurrentInput.bX2Down = false;
    CurrentInput.bX2Released = false;

    CurrentInput.Modifiers = {};
}

void FEditorViewportInputController::RequestContextMenu(const FEditorViewportContextMenuRequest& Request)
{
    PendingContextMenuRequest = Request;
    bHasPendingContextMenuRequest = Request.HitActor || Request.bCanStopPiloting;
}

bool FEditorViewportInputController::ConsumeContextMenuRequest(FEditorViewportContextMenuRequest& OutRequest)
{
    if (!bHasPendingContextMenuRequest)
    {
        return false;
    }

    OutRequest = PendingContextMenuRequest;
    PendingContextMenuRequest = {};
    bHasPendingContextMenuRequest = false;
    return true;
}

bool FEditorViewportInputController::HandleInput(float DeltaTime)
{
    for (const std::unique_ptr<FEditorViewportTool>& Tool : Tools)
    {
        if (Tool && Tool->HandleInput(DeltaTime))
        {
            return true;
        }
    }

    return false;
}
