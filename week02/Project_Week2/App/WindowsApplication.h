#pragma once

#include <windows.h>

class FWindowsApplication
{
public:
    FWindowsApplication();
    ~FWindowsApplication();

public:
    bool Initialize(HINSTANCE hInstance, const wchar_t* WindowTitle, int Width, int Height);
    bool PumpMessages();

    HWND GetHWND() const;
    int GetClientWidth() const;
    int GetClientHeight() const;

    bool ConsumeResizeFlag() const;

    bool ConsumeLeftClick(int& OutX, int& OutY) const;

    bool ConsumeLeftMouseDown(int& OutX, int& OutY) const;
    bool ConsumeLeftMouseUp(int& OutX, int& OutY) const;

    bool IsLeftMousePressed() const;
    void GetMousePosition(int& OutX, int& OutY) const;
    
    // 카메라 조작
    bool ConsumeRightMouseDown(int& OutX, int& OutY) const;
    bool ConsumeRightMouseUp(int& OutX, int& OutY) const;

    bool IsRightMousePressed() const;

    float ConsumeMouseWheelDelta() const;

    bool IsKeyDown(int Key) const;

private:
    static LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

    bool RegisterWindowClass(HINSTANCE hInstance);
    bool CreateAppWindow(HINSTANCE hInstance, const wchar_t* WindowTitle, int Width, int Height);

private:
    HWND hWnd;
    HINSTANCE hInstance;
    int ClientWidth;
    int ClientHeight;
    mutable bool bResized;
    bool bInitialized;

    mutable bool bPendingLeftClick = false;
    mutable int PendingClickX = 0;
    mutable int PendingClickY = 0;

    mutable bool bPendingLeftMouseDown = false;
    mutable bool bPendingLeftMouseUp = false;
    mutable bool bLeftMousePressed = false;

    mutable int MouseX = 0;
    mutable int MouseY = 0;
    mutable int PendingMouseDownX = 0;
    mutable int PendingMouseDownY = 0;
    mutable int PendingMouseUpX = 0;
    mutable int PendingMouseUpY = 0;

    // 카메라 조작
    mutable bool bPendingRightMouseDown = false;
    mutable bool bPendingRightMouseUp = false;
    mutable bool bRightMousePressed = false;

    mutable int PendingRightMouseDownX = 0;
    mutable int PendingRightMouseDownY = 0;
    mutable int PendingRightMouseUpX = 0;
    mutable int PendingRightMouseUpY = 0;

    mutable float MouseWheelDelta = 0.0f;

    bool KeyStates[256] = {};
};