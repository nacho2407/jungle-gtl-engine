#include <Core/CoreMinimal.h>
#include "WindowsWindow.h"

namespace
{
    constexpr const wchar_t* WindowClassName = L"FWindowsApplicationWindowClass";
}

LRESULT CALLBACK AppWndProc(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam);

namespace Engine::ApplicationCore
{
    /**
     * Create the native window.
     *
     * @param InInstance The application instance handle.
     * @param InTitle The window title.
     * @param InWidth The client area width.
     * @param InHeight The client area height.
     * @return true if the window was created, otherwise false.
     */
    bool FWindowsWindow::Create(HINSTANCE InInstance, const wchar_t* InTitle, int32 InWidth,
                                int32 InHeight)
    {
        if (HWnd != nullptr)
        {
            return false;
        }

        WNDCLASSW WindowClass = {};
        WindowClass.lpfnWndProc = AppWndProc;
        WindowClass.hInstance = InInstance;
        WindowClass.lpszClassName = WindowClassName;
        WindowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        RegisterClass(&WindowClass);

        RECT WindowRect = {0, 0, InWidth, InHeight};
        AdjustWindowRect(&WindowRect, WS_OVERLAPPEDWINDOW, FALSE);

        const int32 WindowWidth = WindowRect.right - WindowRect.left;
        const int32 WindowHeight = WindowRect.bottom - WindowRect.top;

        HWnd = CreateWindowExW(
            0, WindowClassName, InTitle, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
            CW_USEDEFAULT, WindowWidth, WindowHeight, nullptr, nullptr, InInstance, this);
        if (HWnd == nullptr)
        {
            return false;
        }

        Width = InWidth;
        Height = InHeight;
        return true;
    }

    void FWindowsWindow::Destroy()
    {
        if (HWnd)
        {
            DestroyWindow(HWnd);
            HWnd = nullptr;
        }
        Width = 0;
        Height = 0;
    }

    void FWindowsWindow::Show()
    {
        if (HWnd != nullptr)
        {
            ShowWindow(HWnd, SW_SHOW);
            UpdateWindow(HWnd);
        }
    }

    void FWindowsWindow::Hide()
    {
        if (HWnd != nullptr)
        {
            ShowWindow(HWnd, SW_HIDE);
        }
    }

} // namespace Engine::ApplicationCore
