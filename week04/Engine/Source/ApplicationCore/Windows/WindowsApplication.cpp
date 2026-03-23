#include "Core/CoreMinimal.h"
#include "WindowsApplication.h"
#include "ApplicationCore/Input/InputSystem.h"
#include <windowsx.h>

namespace Engine::ApplicationCore
{
    namespace
    {
        FWindowsApplication* GWindowsApplication = nullptr;

        int32 GetResizeBorderThicknessX()
        {
            return GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
        }

        int32 GetResizeBorderThicknessY()
        {
            return GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
        }
    } // namespace

    LRESULT CALLBACK AppWndProc(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam)
    {
        if (GWindowsApplication != nullptr)
        {
            LRESULT MessageResult = 0;
            if (Message == WM_NCHITTEST &&
                GWindowsApplication->DispatchMessageHandler(HWnd, Message, WParam, LParam,
                                                            MessageResult))
            {
                return MessageResult;
            }

            if (GWindowsApplication->HandleCustomChromeMessage(HWnd, Message, WParam, LParam,
                                                               MessageResult))
            {
                return MessageResult;
            }

            bool bHandledByApplication = false;
            switch (Message)
            {
            case WM_CLOSE:
                DestroyWindow(HWnd);
                return 0;
            case WM_DESTROY:
                GWindowsApplication->Window.MarkClosed();
                GWindowsApplication->RequestExit();
                PostQuitMessage(0);
                return 0;
            case WM_SIZE:
            {
                int32 NewWidth = static_cast<int32>(LOWORD(LParam));
                int32 NewHeight = static_cast<int32>(HIWORD(LParam));
                GWindowsApplication->OnResizeWindow(NewWidth, NewHeight);
                bHandledByApplication = true;
                break;
            }
            case WM_SETFOCUS:
            {
                GWindowsApplication->OnFocusGained();
                bHandledByApplication = true;
                break;
            }
            case WM_KILLFOCUS:
            {
                GWindowsApplication->OnFocusLost();
                bHandledByApplication = true;
                break;
            }
            }

            if (GWindowsApplication->DispatchMessageHandler(HWnd, Message, WParam, LParam,
                                                            MessageResult))
            {
                return MessageResult;
            }

            if (bHandledByApplication)
            {
                return 0;
            }

            if (FInputSystem* InputSystem = GWindowsApplication->GetInputSystem())
            {
                return InputSystem->ProcessWin32Message(HWnd, Message, WParam, LParam);
            }

            return DefWindowProcW(HWnd, Message, WParam, LParam);
        }

        return DefWindowProcW(HWnd, Message, WParam, LParam);
    }

    FWindowsApplication::FWindowsApplication() { GWindowsApplication = this; }

    FWindowsApplication::~FWindowsApplication()
    {
        if (GWindowsApplication == this)
        {
            GWindowsApplication = nullptr;
        }
    }

    FWindowsApplication* FWindowsApplication::Create() { return new FWindowsApplication(); }

    void FWindowsApplication::SetInputSystem(FInputSystem* InInputSystem)
    {
        InputSystem = InInputSystem;
    }

    FInputSystem* FWindowsApplication::GetInputSystem() const { return InputSystem; }

    bool FWindowsApplication::CreateApplicationWindow(const wchar_t* InTitle, int32 InWidth,
                                                      int32 InHeight)
    {
        HINSTANCE Instance = GetModuleHandleW(nullptr);
        if (!Window.Create(Instance, InTitle, InWidth, InHeight))
        {
            return false;
        }
        RegisterRawMouseInput();
        return true;
    }

    bool FWindowsApplication::CreateEditorWindow(const wchar_t* InTitle, int32 InWidth,
                                                 int32 InHeight)
    {
        HINSTANCE Instance = GetModuleHandleW(nullptr);
        // Editor 전용 창은 기본 title bar를 걷어낸 상태로 생성합니다.
        if (!Window.CreateEditorWindow(Instance, InTitle, InWidth, InHeight))
        {
            return false;
        }

        RegisterRawMouseInput();
        return true;
    }

    void FWindowsApplication::DestroyApplicationWindow() { Window.Destroy(); }

    void FWindowsApplication::OnResizeWindow(int32 NewWidth, int32 NewHeight)
    {
        Window.SetSize(NewWidth, NewHeight);
    }

    void FWindowsApplication::OnFocusGained() {}

    void FWindowsApplication::OnFocusLost() {}

    void FWindowsApplication::PumpMessages()
    {
        MSG Message;

        while (PeekMessageW(&Message, nullptr, 0, 0, PM_REMOVE))
        {
            if (Message.message == WM_QUIT)
            {
                RequestExit();
                continue;
            }

            TranslateMessage(&Message);
            DispatchMessageW(&Message);
        }
    }

    void FWindowsApplication::RequestExit() { bExitRequested = true; }

    void FWindowsApplication::ShowWindow() { Window.Show(); }

    void FWindowsApplication::HideWindow() { Window.Hide(); }

    void* FWindowsApplication::GetNativeWindowHandle() const { return Window.GetHWnd(); }

    void FWindowsApplication::SetMessageHandler(FWindowsMessageHandler InMessageHandler,
                                                void* InUserData)
    {
        MessageHandler = InMessageHandler;
        MessageHandlerUserData = InUserData;
    }

    void FWindowsApplication::SetCustomTitleBarState(const FCustomTitleBarState& InState)
    {
        if (!Window.UsesCustomTitleBar())
        {
            return;
        }

        // ImGui 프레임에서 계산한 버튼 영역을 Win32 hit-test가 바로 읽을 수 있게 저장합니다.
        Window.SetCustomTitleBarState(InState);
    }

    void FWindowsApplication::MinimizeWindow() { Window.Minimize(); }

    void FWindowsApplication::ToggleMaximizeWindow() { Window.ToggleMaximize(); }

    void FWindowsApplication::CloseWindow() { Window.Close(); }

    bool FWindowsApplication::IsWindowMaximized() const
    {
        return Window.IsWindowMaximized();
    }

    const wchar_t* FWindowsApplication::GetWindowTitle() const { return Window.GetTitle().c_str(); }

    void FWindowsApplication::RegisterRawMouseInput()
    {
        RAWINPUTDEVICE Device = {};
        Device.usUsagePage = 0x01;
        Device.usUsage = 0x02;
        Device.dwFlags = 0;
        Device.hwndTarget = Window.GetHWnd();

        bRawMouseInputRegistered = (RegisterRawInputDevices(&Device, 1, sizeof(Device)) == TRUE);
    }

    bool FWindowsApplication::HandleCustomChromeMessage(HWND HWnd, UINT Message, WPARAM WParam,
                                                        LPARAM LParam, LRESULT& OutResult)
    {
        if (!Window.UsesCustomTitleBar())
        {
            return false;
        }

        switch (Message)
        {
        case WM_NCCALCSIZE:
            // non-client 영역 전체를 제거해서 상단 프레임을 우리가 직접 그리게 만듭니다.
            OutResult = 0;
            return true;

        case WM_NCACTIVATE:
            OutResult = 1;
            return true;

        case WM_GETMINMAXINFO:
            // 최대화 시 작업표시줄을 침범하지 않도록 work area 기준으로 크기를 다시 잡습니다.
            UpdateMaximizedBounds(HWnd, reinterpret_cast<MINMAXINFO*>(LParam));
            OutResult = 0;
            return true;

        case WM_NCHITTEST:
            // 가장자리 리사이즈와 상단 드래그 영역을 Win32 native hit-test로 복원합니다.
            OutResult = HitTestCustomChrome(HWnd, LParam);
            return true;

        default:
            return false;
        }
    }

    bool FWindowsApplication::DispatchMessageHandler(HWND HWnd, UINT Message, WPARAM WParam,
                                                     LPARAM LParam, LRESULT& OutResult)
    {
        if (MessageHandler == nullptr)
        {
            return false;
        }

        return MessageHandler(HWnd, Message, WParam, LParam, OutResult, MessageHandlerUserData);
    }

    LRESULT FWindowsApplication::HitTestCustomChrome(HWND HWnd, LPARAM LParam) const
    {
        POINT CursorPosition = {GET_X_LPARAM(LParam), GET_Y_LPARAM(LParam)};
        POINT ClientPosition = CursorPosition;
        ScreenToClient(HWnd, &ClientPosition);

        RECT ClientRect = {};
        GetClientRect(HWnd, &ClientRect);

        const bool bCanResize =
            !Window.IsWindowMaximized() &&
            ((GetWindowLongW(HWnd, GWL_STYLE) & WS_THICKFRAME) != 0);
        if (bCanResize)
        {
            // 테두리 hit-test는 먼저 처리해야 커스텀 타이틀바 위에서도 가장자리 리사이즈가 자연스럽게 동작합니다.
            const int32 ResizeBorderX = GetResizeBorderThicknessX();
            const int32 ResizeBorderY = GetResizeBorderThicknessY();

            const bool bOnLeft = ClientPosition.x >= 0 && ClientPosition.x < ResizeBorderX;
            const bool bOnRight =
                ClientPosition.x < ClientRect.right && ClientPosition.x >= ClientRect.right - ResizeBorderX;
            const bool bOnTop = ClientPosition.y >= 0 && ClientPosition.y < ResizeBorderY;
            const bool bOnBottom =
                ClientPosition.y < ClientRect.bottom && ClientPosition.y >= ClientRect.bottom - ResizeBorderY;

            if (bOnTop && bOnLeft)
            {
                return HTTOPLEFT;
            }
            if (bOnTop && bOnRight)
            {
                return HTTOPRIGHT;
            }
            if (bOnBottom && bOnLeft)
            {
                return HTBOTTOMLEFT;
            }
            if (bOnBottom && bOnRight)
            {
                return HTBOTTOMRIGHT;
            }
            if (bOnLeft)
            {
                return HTLEFT;
            }
            if (bOnRight)
            {
                return HTRIGHT;
            }
            if (bOnTop)
            {
                return HTTOP;
            }
            if (bOnBottom)
            {
                return HTBOTTOM;
            }
        }

        const FCustomTitleBarState& TitleBarState = Window.GetCustomTitleBarState();
        if (TitleBarState.TitleBarHeight > 0 && ClientPosition.y >= 0 &&
            ClientPosition.y < TitleBarState.TitleBarHeight)
        {
            for (const FWindowHitTestRect& InteractiveRect : TitleBarState.InteractiveRects)
            {
                if (InteractiveRect.Contains(ClientPosition.x, ClientPosition.y))
                {
                    // 버튼 위에서는 드래그를 막고 ImGui가 클릭 입력을 그대로 받게 합니다.
                    return HTCLIENT;
                }
            }

            // 나머지 상단 영역은 caption으로 취급해서 창 이동/더블클릭 최대화를 Win32에 맡깁니다.
            return HTCAPTION;
        }

        return HTCLIENT;
    }

    void FWindowsApplication::UpdateMaximizedBounds(HWND HWnd,
                                                    MINMAXINFO* InOutMinMaxInfo) const
    {
        if (InOutMinMaxInfo == nullptr)
        {
            return;
        }

        HMONITOR Monitor = MonitorFromWindow(HWnd, MONITOR_DEFAULTTONEAREST);
        if (Monitor == nullptr)
        {
            return;
        }

        MONITORINFO MonitorInfo = {};
        MonitorInfo.cbSize = sizeof(MONITORINFO);
        if (!GetMonitorInfoW(Monitor, &MonitorInfo))
        {
            return;
        }

        const RECT& MonitorRect = MonitorInfo.rcMonitor;
        const RECT& WorkRect = MonitorInfo.rcWork;

        InOutMinMaxInfo->ptMaxPosition.x = WorkRect.left - MonitorRect.left;
        InOutMinMaxInfo->ptMaxPosition.y = WorkRect.top - MonitorRect.top;
        InOutMinMaxInfo->ptMaxSize.x = WorkRect.right - WorkRect.left;
        InOutMinMaxInfo->ptMaxSize.y = WorkRect.bottom - WorkRect.top;
        InOutMinMaxInfo->ptMaxTrackSize = InOutMinMaxInfo->ptMaxSize;
    }
} // namespace Engine::ApplicationCore
