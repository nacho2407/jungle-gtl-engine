#pragma once
#include "Engine/Input/Controller/EditorController/EditorWorldController.h"
#include "Engine/Input/Controller/EditorController/PIEController.h"

enum class EActiveEditorController
{
    EditorWorldController,
    PIEController,
    NilController,
};

enum class EKeyInputType
{
    KeyPressed,
    KeyDown,
    KeyReleased,
    KeyNone,
};

enum class EMouseInputType
{
    E_MouseMoved,         // delta movement (DX, DY)
    E_MouseMovedAbsolute, // viewport-local absolute position (X, Y)
    E_LeftMouseClicked,   // LMB pressed down
    E_LeftMouseDragged,   // LMB held + drag threshold met
    E_LeftMouseDragEnded, // LMB drag released
    E_LeftMouseButtonUp,  // LMB released (no drag / below threshold)
    E_RightMouseClicked,
    E_RightMouseDragged,
    E_MiddleMouseDragged,
    E_MouseWheelScrolled,
};

class FEditorInputRouter
{
  public:
    FEditorInputRouter() = default;
    ~FEditorInputRouter() = default;

    void Tick(float DeltaTime);
    void RouteKeyboardInput(EKeyInputType Type, int VK);
    void RouteMouseInput(EMouseInputType Type, float DeltaX, float DeltaY);

    EActiveEditorController GetActiveController() { return ActiveEditorControllerState; }
    void                    SetActiveController(EActiveEditorController);
    void                    SetViewportDim(float X, float Y, float Width, float Height);

    FEditorWorldController& GetEditorWorldController() { return EditorWorldController; }
    FPIEController&         GetPIEController() { return PIEController; }

  private:
    EActiveEditorController ActiveEditorControllerState = EActiveEditorController::EditorWorldController;
    IBaseEditorController*  ActiveController = nullptr;
    FEditorWorldController  EditorWorldController;
    FPIEController          PIEController;
};