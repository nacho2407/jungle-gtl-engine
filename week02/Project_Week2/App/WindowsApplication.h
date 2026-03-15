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
};