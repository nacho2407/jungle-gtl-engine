#pragma once
#include "Editor/Input/EditorWorldController.h"
#include "Editor/Input/GameInputBridge.h"

enum class EActiveEditorController
{
    EditorWorldController,
    GameInputBridge,
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

    EActiveEditorController GetActiveController() const { return ActiveEditorControllerState; }
    void                    SetActiveController(EActiveEditorController);
    void                    SetViewportDim(float X, float Y, float Width, float Height);

    FEditorWorldController& GetEditorWorldController() { return EditorWorldController; }
    const FEditorWorldController& GetEditorWorldController() const { return EditorWorldController; }
    FGameInputBridge&         GetGameInputBridge() { return GameInputBridge; }
    const FGameInputBridge&   GetGameInputBridge() const { return GameInputBridge; }

  private:
    EActiveEditorController ActiveEditorControllerState = EActiveEditorController::EditorWorldController;
    IBaseEditorController*  ActiveController = nullptr;
    FEditorWorldController  EditorWorldController;
    FGameInputBridge          GameInputBridge;
};
