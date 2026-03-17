#include "FInputManager.h"
#include "WindowsApplication.h"
#include "FGUIManager.h"
#include "imgui.h"

bool FInputManager::Initialize(FWindowsApplication* InWindowApp, FGUIManager* InGUIManager)
{
    WindowApp = InWindowApp;
    GUIManager = InGUIManager;

    MouseX = MouseY = 0;
    PrevMouseX = PrevMouseY = 0;
    MouseDeltaX = MouseDeltaY = 0;
    MouseWheelDelta = 0.0f;

    for (int i = 0; i < (int)EMouseButton::Count; ++i)
    {
        MouseButtons[i] = FButtonState{};
    }

    return WindowApp != nullptr;
}

void FInputManager::BeginFrame()
{
    if (!WindowApp)
    {
        return;
    }

    for (int i = 0; i < (int)EMouseButton::Count; ++i)
    {
        MouseButtons[i].bPressed = false;
        MouseButtons[i].bReleased = false;
    }

    PrevMouseX = MouseX;
    PrevMouseY = MouseY;
    WindowApp->GetMousePosition(MouseX, MouseY);

    MouseDeltaX = MouseX - PrevMouseX;
    MouseDeltaY = MouseY - PrevMouseY;

    MouseWheelDelta = WindowApp->ConsumeMouseWheelDelta();

    int DummyX = 0;
    int DummyY = 0;

    if (WindowApp->ConsumeLeftMouseDown(DummyX, DummyY))
    {
        MouseButtons[(int)EMouseButton::Left].bDown = true;
        MouseButtons[(int)EMouseButton::Left].bPressed = true;
    }

    if (WindowApp->ConsumeLeftMouseUp(DummyX, DummyY))
    {
        MouseButtons[(int)EMouseButton::Left].bDown = false;
        MouseButtons[(int)EMouseButton::Left].bReleased = true;
    }

    if (WindowApp->ConsumeRightMouseDown(DummyX, DummyY))
    {
        MouseButtons[(int)EMouseButton::Right].bDown = true;
        MouseButtons[(int)EMouseButton::Right].bPressed = true;
    }

    if (WindowApp->ConsumeRightMouseUp(DummyX, DummyY))
    {
        MouseButtons[(int)EMouseButton::Right].bDown = false;
        MouseButtons[(int)EMouseButton::Right].bReleased = true;
    }
}

void FInputManager::EndFrame()
{
}

bool FInputManager::CanProcessMouse() const
{
    if (ImGui::GetCurrentContext() == nullptr)
    {
        return true;
    }

    return !ImGui::GetIO().WantCaptureMouse;
}

bool FInputManager::CanProcessKeyboard() const
{
    if (ImGui::GetCurrentContext() == nullptr)
    {
        return true;
    }

    return !ImGui::GetIO().WantCaptureKeyboard;
}

bool FInputManager::IsMouseDown(EMouseButton Button) const
{
    return MouseButtons[(int)Button].bDown;
}

bool FInputManager::WasMousePressed(EMouseButton Button) const
{
    return MouseButtons[(int)Button].bPressed;
}

bool FInputManager::WasMouseReleased(EMouseButton Button) const
{
    return MouseButtons[(int)Button].bReleased;
}

void FInputManager::GetMousePosition(int& OutX, int& OutY) const
{
    OutX = MouseX;
    OutY = MouseY;
}

void FInputManager::GetMouseDelta(int& OutDeltaX, int& OutDeltaY) const
{
    OutDeltaX = MouseDeltaX;
    OutDeltaY = MouseDeltaY;
}

float FInputManager::GetMouseWheelDelta() const
{
    return MouseWheelDelta;
}

bool FInputManager::IsKeyDown(int Key) const
{
    if (!WindowApp)
    {
        return false;
    }

    return WindowApp->IsKeyDown(Key);
}