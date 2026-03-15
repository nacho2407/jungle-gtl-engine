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
};