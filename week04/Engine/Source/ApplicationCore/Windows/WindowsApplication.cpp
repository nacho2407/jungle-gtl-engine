#include <Core/CoreMinimal.h>
#include "WindowsApplication.h"
#include "ApplicationCore/Input/InputSystem.h"

// #include <windowsx.h>

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
            return GWindowsApplication->GetInputSystem()->ProcessWin32Message(
                HWnd, Message, WParam, LParam);
        }
        return DefWindowProcW(HWnd, Message, WParam, LParam);
    }

    FWindowsApplication::FWindowsApplication()
    {
        GWindowsApplication = this;
    }

    FWindowsApplication::~FWindowsApplication()
    {
        if (GWindowsApplication == this)
        {
            GWindowsApplication = nullptr;
        }
    }

    FWindowsApplication* FWindowsApplication::Create()
    {
        return new FWindowsApplication();
    }

    void FWindowsApplication::SetInputSystem(FInputSystem* InInputSystem)
    {
        InputSystem = InInputSystem;
    }

    FInputSystem* FWindowsApplication::GetInputSystem() const
    {
        return InputSystem;
    }

    bool FWindowsApplication::CreateApplicationWindow(const wchar_t* InTitle, int32 InWidth,
                                                      int32          InHeight)
    {
        HINSTANCE Instance = GetModuleHandleW(nullptr);
        if (!Window.Create(Instance, InTitle, InWidth, InHeight))
        {
            return false;
        }
        RegisterRawMouseInput();
        return true;
    }

    void FWindowsApplication::DestroyApplicationWindow()
    {
        Window.Destroy();
    }

    void FWindowsApplication::PumpMessages()
    {
        MSG Message;

        while (PeekMessageW(&Message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&Message);
            DispatchMessageW(&Message);
        }
    }

    void FWindowsApplication::ShowWindow()
    {
        Window.Show();
    }

    void FWindowsApplication::HideWindow()
    {
        Window.Hide();
    }

    void* FWindowsApplication::GetNativeWindowHandle() const
    {
        return Window.GetHWnd();
    }

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