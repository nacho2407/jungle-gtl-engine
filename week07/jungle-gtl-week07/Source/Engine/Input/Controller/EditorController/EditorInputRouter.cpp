#include "EditorInputRouter.h"

void FEditorInputRouter::SetActiveController(EActiveEditorController Controller)
{
    ActiveEditorControllerState = Controller;
    switch (Controller)
    {
    case (EActiveEditorController::EditorWorldController):
        ActiveController = &EditorWorldController;
        break;
    case (EActiveEditorController::PIEController):
        ActiveController = &PIEController;
        break;
    case (EActiveEditorController::NilController):
        ActiveController = nullptr;
        break;
    }
}

void FEditorInputRouter::Tick(float DeltaTime)
{
    if (ActiveController)
        ActiveController->Tick(DeltaTime);
}

void FEditorInputRouter::SetViewportDim(float X, float Y, float Width, float Height)
{
    EditorWorldController.SetViewportDim(X, Y, Width, Height);
    PIEController.SetViewportDim(X, Y, Width, Height);
}

void FEditorInputRouter::RouteKeyboardInput(EKeyInputType Type, int VK)
{
    if (!ActiveController)
        return;
    switch (Type)
    {
    case (EKeyInputType::KeyPressed):
        ActiveController->OnKeyPressed(VK);
        break;
    case (EKeyInputType::KeyDown):
        ActiveController->OnKeyDown(VK);
        break;
    case (EKeyInputType::KeyReleased):
        ActiveController->OnKeyReleased(VK);
        break;
    case (EKeyInputType::KeyNone):
        break;
    }
}

void FEditorInputRouter::RouteMouseInput(EMouseInputType Type, float DeltaX, float DeltaY)
{
    if (!ActiveController)
        return;
    switch (Type)
    {
    case (EMouseInputType::E_MouseMoved):
        ActiveController->OnMouseMove(DeltaX, DeltaY);
        break;
    case (EMouseInputType::E_MouseMovedAbsolute):
        ActiveController->OnMouseMoveAbsolute(DeltaX, DeltaY);
        break;
    case (EMouseInputType::E_LeftMouseClicked):
        ActiveController->OnLeftMouseClick(DeltaX, DeltaY);
        break;
    case (EMouseInputType::E_LeftMouseDragEnded):
        ActiveController->OnLeftMouseDragEnd(DeltaX, DeltaY);
        break;
    case (EMouseInputType::E_LeftMouseButtonUp):
        ActiveController->OnLeftMouseButtonUp(DeltaX, DeltaY);
        break;
    case (EMouseInputType::E_RightMouseClicked):
        ActiveController->OnRightMouseClick(DeltaX, DeltaY);
        break;
    case (EMouseInputType::E_LeftMouseDragged):
        ActiveController->OnLeftMouseDrag(DeltaX, DeltaY);
        break;
    case (EMouseInputType::E_RightMouseDragged):
        ActiveController->OnRightMouseDrag(DeltaX, DeltaY);
        break;
    case (EMouseInputType::E_MiddleMouseDragged):
        ActiveController->OnMiddleMouseDrag(DeltaX, DeltaY);
        break;
    case (EMouseInputType::E_MouseWheelScrolled):
        // Use Delta X for Scroll Notch
        ActiveController->OnWheelScrolled(DeltaX);
        break;
    }
}