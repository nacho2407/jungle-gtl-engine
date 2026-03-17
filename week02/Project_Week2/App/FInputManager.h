#pragma once

class FWindowsApplication;
class FGUIManager;

enum class EMouseButton
{
    Left = 0,
    Right,
    Middle,
    Count
};

struct FButtonState
{
    bool bDown = false;       // 현재 눌림 유지
    bool bPressed = false;    // 이번 프레임에 눌림
    bool bReleased = false;   // 이번 프레임에 떼짐
};

class FInputManager
{
public:
    bool Initialize(FWindowsApplication* InWindowApp, FGUIManager* InGUIManager);

    void BeginFrame();
    void EndFrame();

    bool CanProcessMouse() const;
    bool CanProcessKeyboard() const;

    bool IsMouseDown(EMouseButton Button) const;
    bool WasMousePressed(EMouseButton Button) const;
    bool WasMouseReleased(EMouseButton Button) const;

    void GetMousePosition(int& OutX, int& OutY) const;
    void GetMouseDelta(int& OutDX, int& OutDY) const;
    float GetMouseWheelDelta() const;

    bool IsKeyDown(int Key) const;

private:
    FWindowsApplication* WindowApp = nullptr;
    FGUIManager* GUIManager = nullptr;

    FButtonState MouseButtons[(int)EMouseButton::Count];

    int MouseX = 0;
    int MouseY = 0;
    int PrevMouseX = 0;
    int PrevMouseY = 0;

    int MouseDeltaX = 0;
    int MouseDeltaY = 0;

    float MouseWheelDelta = 0.0f;
};