#include "Core/CoreMinimal.h"
#include "WindowsApplication.h"
#include "ApplicationCore/Input/InputSystem.h"


namespace Engine::ApplicationCore
{
    namespace
    {
        FWindowsApplication* GWindowsApplication = nullptr;
    } // namespace

    LRESULT CALLBACK AppWndProc(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam)
    {
        if (GWindowsApplication != nullptr)
        {
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
                return 0;
            }
            case WM_SETFOCUS:
                GWindowsApplication->OnFocusGained();
                return 0;
            case WM_KILLFOCUS:
                GWindowsApplication->OnFocusLost();
                return 0;
            }


            if (FInputSystem* InputSystem = GWindowsApplication->GetInputSystem())
            {
                return InputSystem->ProcessWin32Message(HWnd, Message, WParam, LParam);
            }
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

    void FWindowsApplication::RegisterRawMouseInput()
    {
        RAWINPUTDEVICE Device = {};
        Device.usUsagePage = 0x01;
        Device.usUsage = 0x02;
        Device.dwFlags = 0;
        Device.hwndTarget = Window.GetHWnd();

        bRawMouseInputRegistered = (RegisterRawInputDevices(&Device, 1, sizeof(Device)) == TRUE);
    }
} // namespace Engine::ApplicationCore
