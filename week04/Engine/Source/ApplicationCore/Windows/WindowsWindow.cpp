#include "Core/CoreMinimal.h"
#include "WindowsWindow.h"

namespace Engine::ApplicationCore
{
    namespace
    {
        constexpr const wchar_t* WindowClassName = L"FWindowsApplicationWindowClass";
        constexpr int32 DefaultCustomTitleBarHeight = 36;
    }

    LRESULT CALLBACK AppWndProc(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam);

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
        return CreateInternal(InInstance, InTitle, InWidth, InHeight, WS_OVERLAPPEDWINDOW, false);
    }

    bool FWindowsWindow::CreateEditorWindow(HINSTANCE InInstance, const wchar_t* InTitle,
                                            int32 InWidth, int32 InHeight)
    {
        const DWORD EditorWindowStyle = WS_OVERLAPPEDWINDOW & ~WS_CAPTION;
        return CreateInternal(InInstance, InTitle, InWidth, InHeight, EditorWindowStyle, true);
    }

    bool FWindowsWindow::CreateInternal(HINSTANCE InInstance, const wchar_t* InTitle, int32 InWidth,
                                        int32 InHeight, DWORD InWindowStyle,
                                        bool bInUseCustomTitleBar)
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

        if (!RegisterClassW(&WindowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            return false;
        }

        RECT WindowRect = {0, 0, InWidth, InHeight};
        AdjustWindowRectEx(&WindowRect, InWindowStyle, FALSE, 0);

        const int32 WindowWidth = WindowRect.right - WindowRect.left;
        const int32 WindowHeight = WindowRect.bottom - WindowRect.top;

        Width = InWidth;
        Height = InHeight;
        Title = (InTitle != nullptr) ? InTitle : L"";
        bIsVisible = false;
        bIsClosed = false;
        bUsesCustomTitleBar = bInUseCustomTitleBar;
        CustomTitleBarState = {};
        if (bUsesCustomTitleBar)
        {
            CustomTitleBarState.TitleBarHeight = DefaultCustomTitleBarHeight;
        }

        HWnd = CreateWindowExW(0, WindowClassName, InTitle, InWindowStyle | WS_VISIBLE,
                               CW_USEDEFAULT, CW_USEDEFAULT, WindowWidth, WindowHeight, nullptr,
                               nullptr, InInstance, this);

        if (HWnd == nullptr)
        {
            Width = 0;
            Height = 0;
            Title.clear();
            bUsesCustomTitleBar = false;
            CustomTitleBarState = {};
            return false;
        }

        bIsVisible = true;
        if (bUsesCustomTitleBar)
        {
            SetWindowPos(HWnd, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                             SWP_FRAMECHANGED);
        }

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
            bIsVisible = true;
        }
    }

    void FWindowsWindow::Hide()
    {
        if (HWnd != nullptr)
        {
            ShowWindow(HWnd, SW_HIDE);
            bIsVisible = false;
        }
    }

    void FWindowsWindow::Close()
    {
        if (HWnd != nullptr)
        {
            PostMessageW(HWnd, WM_CLOSE, 0, 0);
        }
    }

    void FWindowsWindow::Minimize()
    {
        if (HWnd != nullptr)
        {
            ShowWindow(HWnd, SW_MINIMIZE);
        }
    }

    void FWindowsWindow::Maximize()
    {
        if (HWnd != nullptr)
        {
            ShowWindow(HWnd, SW_MAXIMIZE);
        }
    }

    void FWindowsWindow::Restore()
    {
        if (HWnd != nullptr)
        {
            ShowWindow(HWnd, SW_RESTORE);
        }
    }

    void FWindowsWindow::ToggleMaximize()
    {
        if (IsWindowMaximized())
        {
            Restore();
            return;
        }

        Maximize();
    }

    bool FWindowsWindow::IsWindowMaximized() const
    {
        return HWnd != nullptr && IsZoomed(HWnd) != FALSE;
    }
} // namespace Engine::ApplicationCore
