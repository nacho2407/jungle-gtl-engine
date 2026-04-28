#pragma once

#include <memory>

#include "Core/CoreTypes.h"
#include "EditorViewportTool.h"
#include "Input/InputTypes.h"

class AActor;
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

/*
 * note: 현재는 pilot 관련 context menu 요청만 고려하여 설계하였지만, 향후 다른 종류의 요청이 추가되는 경우 이 구조체의 내용을 수정하여 사용해야 함
 */
struct FEditorViewportContextMenuRequest
{
    AActor* HitActor = nullptr;

    POINT ScreenPos = { 0, 0 };
    POINT ClientPos = { 0, 0 };
    POINT LocalPos = { 0, 0 };

    bool bCanStopPiloting = false;
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

    void RequestContextMenu(const FEditorViewportContextMenuRequest& Request);
    bool ConsumeContextMenuRequest(FEditorViewportContextMenuRequest& OutRequest);

    bool HandleInput(float DeltaTime);

private:
    FEditorViewportClient* Owner = nullptr;
    FEditorViewportFrameInput CurrentInput;
    FEditorViewportContextMenuRequest PendingContextMenuRequest;
    bool bHasPendingContextMenuRequest = false;

    TArray<std::unique_ptr<FEditorViewportTool>> Tools;
};
