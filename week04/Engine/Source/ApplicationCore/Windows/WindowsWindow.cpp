#include <Core/CoreMinimal.h>
#include "WindowsWindow.h"

Engine::ApplicationCore::FWindowsWindow::FWindowsWindow() {}

Engine::ApplicationCore::FWindowsWindow::~FWindowsWindow() {}

bool Engine::ApplicationCore::FWindowsWindow::Initialize(const wchar_t* InTitle, int32 InWidth,
                                                         int32 InHeight)
{
    WCHAR     WindowClass[] = L"JungleWindowClass";
    WCHAR     Title[] = L"Custom Engine";
    WNDCLASSW WndClass = {0, FWindowsWindow::WindowProc, 0, 0, 0, 0, 0, 0, 0, WindowClass};

    RegisterClass(&WndClass);

    HWnd =
        CreateWindowExW(0, WindowClass, Title, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT, CW_USEDEFAULT, 1920, 1080, nullptr, nullptr, nullptr, this);
}

void Engine::ApplicationCore::FWindowsWindow::Destroy()
{
    if (HWnd)
    {
        DestroyWindow(HWnd);
        HWnd = nullptr;
    }
}

LRESULT CALLBACK Engine::ApplicationCore::FWindowsWindow::WindowProc(HWND hWnd, UINT message,
                                                                     WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        break;
    }
    case WM_NCCREATE:
    {
        LPCREATESTRUCT pcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        break;
    }
}