#include <windowsx.h>

#include "WindowsApplication.h"

FWindowsApplication::FWindowsApplication()
    : hWnd(nullptr)
    , hInstance(nullptr)
    , ClientWidth(0)
    , ClientHeight(0)
    , bResized(false)
    , bInitialized(false)
{
}

FWindowsApplication::~FWindowsApplication()
{
    if (hWnd)
    {
        DestroyWindow(hWnd);
        hWnd = nullptr;
    }
}

bool FWindowsApplication::Initialize(HINSTANCE InHInstance, const wchar_t* WindowTitle, int Width, int Height)
{
    hInstance = InHInstance;
    ClientWidth = Width;
    ClientHeight = Height;

    if (!RegisterWindowClass(hInstance))
    {
        return false;
    }

    if (!CreateAppWindow(hInstance, WindowTitle, Width, Height))
    {
        return false;
    }

    bInitialized = true;
    return true;
}

bool FWindowsApplication::RegisterWindowClass(HINSTANCE InHInstance)
{
    WNDCLASSEXW Wc = {};
    Wc.cbSize = sizeof(WNDCLASSEXW);
    Wc.style = CS_HREDRAW | CS_VREDRAW;
    Wc.lpfnWndProc = FWindowsApplication::StaticWndProc;
    Wc.hInstance = InHInstance;
    Wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    Wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    Wc.lpszClassName = L"MyEngineWindowClass";

    return RegisterClassExW(&Wc) != 0;
}

bool FWindowsApplication::CreateAppWindow(HINSTANCE InHInstance, const wchar_t* WindowTitle, int Width, int Height)
{
    RECT Rc = { 0, 0, Width, Height };
    AdjustWindowRect(&Rc, WS_OVERLAPPEDWINDOW, FALSE);

    hWnd = CreateWindowExW(
        0,
        L"MyEngineWindowClass",
        WindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        Rc.right - Rc.left,
        Rc.bottom - Rc.top,
        nullptr,
        nullptr,
        InHInstance,
        this);

    if (!hWnd)
    {
        return false;
    }

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    return true;
}

bool FWindowsApplication::PumpMessages()
{
    MSG Msg = {};

    while (PeekMessageW(&Msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (Msg.message == WM_QUIT)
        {
            return false;
        }

        TranslateMessage(&Msg);
        DispatchMessageW(&Msg);
    }

    return true;
}

HWND FWindowsApplication::GetHWND() const
{
    return hWnd;
}

int FWindowsApplication::GetClientWidth() const
{
    return ClientWidth;
}

int FWindowsApplication::GetClientHeight() const
{
    return ClientHeight;
}

bool FWindowsApplication::ConsumeResizeFlag() const
{
    const bool bWasResized = bResized;
    bResized = false;
    return bWasResized;
}

bool FWindowsApplication::ConsumeLeftClick(int& OutX, int& OutY) const
{
    if (!bPendingLeftClick)
    {
        return false;
    }

    OutX = PendingClickX;
    OutY = PendingClickY;
    bPendingLeftClick = false;
    return true;
}

bool FWindowsApplication::ConsumeLeftMouseDown(int& OutX, int& OutY) const
{
    if (!bPendingLeftMouseDown)
    {
        return false;
    }

    OutX = PendingMouseDownX;
    OutY = PendingMouseDownY;
    bPendingLeftMouseDown = false;
    return true;
}

bool FWindowsApplication::ConsumeLeftMouseUp(int& OutX, int& OutY) const
{
    if (!bPendingLeftMouseUp)
    {
        return false;
    }

    OutX = PendingMouseUpX;
    OutY = PendingMouseUpY;
    bPendingLeftMouseUp = false;
    return true;
}

bool FWindowsApplication::IsLeftMousePressed() const
{
    return bLeftMousePressed;
}

void FWindowsApplication::GetMousePosition(int& OutX, int& OutY) const
{
    OutX = MouseX;
    OutY = MouseY;
}

LRESULT CALLBACK FWindowsApplication::StaticWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    FWindowsApplication* App = nullptr;

    if (Msg == WM_NCCREATE)
    {
        CREATESTRUCTW* CreateStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        App = reinterpret_cast<FWindowsApplication*>(CreateStruct->lpCreateParams);

        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(App));
    }
    else
    {
        App = reinterpret_cast<FWindowsApplication*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (App)
    {
        return App->WndProc(hWnd, Msg, wParam, lParam);
    }

    return DefWindowProcW(hWnd, Msg, wParam, lParam);
}

LRESULT FWindowsApplication::WndProc(HWND InHWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    switch (Msg)
    {
    case WM_SIZE:
    {
        ClientWidth = LOWORD(lParam);
        ClientHeight = HIWORD(lParam);
        bResized = true;
        return 0;
    }
    case WM_KEYDOWN:
    {
        if (wParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
        }
        return 0;
    }
    case WM_LBUTTONDOWN:
    {
        MouseX = GET_X_LPARAM(lParam);
        MouseY = GET_Y_LPARAM(lParam);

        PendingMouseDownX = MouseX;
        PendingMouseDownY = MouseY;
        bPendingLeftMouseDown = true;
        bLeftMousePressed = true;

        SetCapture(InHWnd);
        return 0;
    }
    case WM_LBUTTONUP:
    {
        MouseX = GET_X_LPARAM(lParam);
        MouseY = GET_Y_LPARAM(lParam);

        PendingMouseUpX = MouseX;
        PendingMouseUpY = MouseY;
        bPendingLeftMouseUp = true;
        bLeftMousePressed = false;

        ReleaseCapture();
        return 0;
    }
    case WM_MOUSEMOVE:
    {
        MouseX = GET_X_LPARAM(lParam);
        MouseY = GET_Y_LPARAM(lParam);
        return 0;
    }
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProcW(InHWnd, Msg, wParam, lParam);
}