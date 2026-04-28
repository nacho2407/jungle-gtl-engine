#pragma once

#include <memory>

#include "Core/CoreTypes.h"
#include "EditorViewportTool.h"
#include "Input/InputTypes.h"

class FEditorViewportClient;

/**
 * @brief FEditorViewportInputController가 한 프레임 동안 받은 viewport 입력 event를 저장하는 구조체
 */
struct FEditorViewportFrameInput
{
    bool KeyDown[256] = {};
    bool KeyPressed[256] = {};
    bool KeyReleased[256] = {};
    bool KeyRepeated[256] = {};

    POINT MouseLocalPos = { 0, 0 };
    POINT MouseClientPos = { 0, 0 };
    POINT MouseScreenPos = { 0, 0 };

    POINT MouseAxisDelta = { 0, 0 };
    float MouseWheelNotches = 0.0f;

    bool bLeftPressed = false;
    bool bLeftDown = false;
    bool bLeftReleased = false;

    bool bRightPressed = false;
    bool bRightDown = false;
    bool bRightReleased = false;

    bool bMiddlePressed = false;
    bool bMiddleDown = false;
    bool bMiddleReleased = false;

    FInputModifiers Modifiers;
};

class FEditorViewportInputController
{
public:
    explicit FEditorViewportInputController(FEditorViewportClient* InOwner);

    bool InputKey(const FViewportKeyEvent& Event);
    bool InputAxis(const FViewportAxisEvent& Event);
    bool InputPointer(const FViewportPointerEvent& Event);

    void BeginInputFrame();
    const FEditorViewportFrameInput& GetCurrentInput() const { return CurrentInput; }

    bool HandleInput(float DeltaTime);

private:
    FEditorViewportClient* Owner = nullptr;
    FEditorViewportFrameInput CurrentInput;

    TArray<std::unique_ptr<FEditorViewportTool>> Tools;
};
