#include <Core/CoreMinimal.h>
#include "WindowsApplication.h"

#include <Core/CoreMinimal.h>
#include <windowsx.h>
#include "WindowsApplication.h"

namespace Engine::ApplicationCore
{
    namespace
    {
        FWindowsApplication* GWindowsApplication = nullptr;

        int GetMouseX(LPARAM InLParam) { return static_cast<int32>(GET_X_LPARAM(InLParam)); }
        int GetMouseY(LPARAM InLParam) { return static_cast<int32>(GET_Y_LPARAM(InLParam)); }
    } // namespace

    LRESULT CALLBACK AppWndProc(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam)
    {
        if (GWindowsApplication != nullptr)
        {
            return GWindowsApplication->ProcessMessage(HWnd, Message, WParam, LParam);
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

    void FWindowsApplication::SetMessageHandler(IApplicationMessageHandler* InMessageHandler)
    {
        MessageHandler = InMessageHandler;
    }

    IApplicationMessageHandler* FWindowsApplication::GetMessageHandler() const
    {
        return MessageHandler;
    }

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

    void FWindowsApplication::PumpMessages()
    {
        MSG Message;

        while (PeekMessageW(&Message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&Message);
            DispatchMessageW(&Message);
        }
    }

    void FWindowsApplication::ShowWindow() { Window.Show(); }
    void FWindowsApplication::HideWindow() { Window.Hide(); }

    void* FWindowsApplication::GetNativeWindowHandle() const { return Window.GetHWnd(); }

    bool FWindowsApplication::ProcessKeyDownEvent(EKey Key, bool bIsRepeat)
    {
        return MessageHandler ? MessageHandler->OnKeyDown(Key, bIsRepeat) : false;
    }

    bool FWindowsApplication::ProcessKeyUpEvent(EKey Key)
    {
        return MessageHandler ? MessageHandler->OnKeyUp(Key) : false;
    }

    bool FWindowsApplication::ProcessMouseDownEvent(EKey Button, int32 X, int32 Y)
    {
        return MessageHandler ? MessageHandler->OnMouseDown(Button, X, Y) : false;
    }

    bool FWindowsApplication::ProcessMouseUpEvent(EKey Button, int32 X, int32 Y)
    {
        return MessageHandler ? MessageHandler->OnMouseUp(Button, X, Y) : false;
    }
    bool FWindowsApplication::ProcessMouseDoubleClickEvent(EKey Button, int32 X, int32 Y)
    {
        return MessageHandler ? MessageHandler->OnMouseDoubleClick(Button, X, Y) : false;
    }

    bool FWindowsApplication::ProcessMouseMoveEvent(int32 X, int32 Y)
    {
        return MessageHandler ? MessageHandler->OnMouseMove(X, Y) : false;
    }

    bool FWindowsApplication::ProcessRawMouseMoveEvent(int32 DeltaX, int32 DeltaY)
    {
        return MessageHandler ? MessageHandler->OnRawMouseMove(DeltaX, DeltaY) : false;
    }

    bool FWindowsApplication::ProcessMouseWheelEvent(float Delta, int32 X, int32 Y)
    {
        return MessageHandler ? MessageHandler->OnMouseWheel(Delta, X, Y) : false;
    }

    LRESULT FWindowsApplication::ProcessMessage(HWND InHWnd, UINT InMessage, WPARAM InWParam,
                                                LPARAM InLParam)
    {
        switch (InMessage)
        {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            const EKey Key = TranslateKey(InWParam);
            if (Key != EKey::Invalid)
            {
                const bool bIsRepeat = (InLParam & (1 << 30)) != 0;
                ProcessKeyDownEvent(Key, bIsRepeat);
            }
            return 0;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            const EKey Key = TranslateKey(InWParam);
            ProcessKeyUpEvent(Key);
            return 0;
        }
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        {
            const EKey  Button = TranslateMouseButton(InMessage);
            const int32 X = GetMouseX(InLParam);
            const int32 Y = GetMouseY(InLParam);

            SetCapture(InHWnd);
            LastMouseX = X;
            LastMouseY = Y;
            bHasLastMousePosition = true;
            ProcessMouseDownEvent(Button, X, Y);
            return 0;
        }
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        {
            const EKey  Button = TranslateMouseButton(InMessage);
            const int32 X = GetMouseX(InLParam);
            const int32 Y = GetMouseY(InLParam);

            const bool bOtherButtonsStillHeld =
                (InWParam & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON)) != 0;
            if (!bOtherButtonsStillHeld)
            {
                ReleaseCapture();
            }
            ProcessMouseUpEvent(Button, X, Y);
            return 0;
        }
        case WM_KILLFOCUS:
        case WM_CANCELMODE:
        {
            ReleaseCapture();
            bHasLastMousePosition = false;
            if (MessageHandler != nullptr)
            {
                MessageHandler->OnFocusLost();
            }
            return 0;
        }
        case WM_CAPTURECHANGED:
        {
            bHasLastMousePosition = false;
            return 0;
        }
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDBLCLK:
        {
            const EKey  Button = TranslateMouseButton(InMessage);
            const int32 X = GetMouseX(InLParam);
            const int32 Y = GetMouseY(InLParam);

            ProcessMouseDoubleClickEvent(Button, X, Y);
            return 0;
        }
        case WM_MOUSEMOVE:
        {
            const int32 X = GetMouseX(InLParam);
            const int32 Y = GetMouseY(InLParam);

            ProcessMouseMoveEvent(X, Y);

            const bool bHasButtonCapture = (InWParam & (MK_RBUTTON | MK_MBUTTON)) != 0;
            if (!bRawMouseInputRegistered && bHasButtonCapture)
            {
                if (bHasLastMousePosition)
                {
                    const int32 DeltaX = X - LastMouseX;
                    const int32 DeltaY = Y - LastMouseY;
                    ProcessRawMouseMoveEvent(DeltaX, DeltaY);
                }
            }
            LastMouseX = X;
            LastMouseY = Y;
            bHasLastMousePosition = true;
            return 0;
        }
        case WM_MOUSEWHEEL:
        {
            POINT ScreenPoint;
            ScreenPoint.x = GET_X_LPARAM(InLParam);
            ScreenPoint.y = GET_Y_LPARAM(InLParam);
            ScreenToClient(InHWnd, &ScreenPoint);

            const float Delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(InWParam)) /
                                static_cast<float>(WHEEL_DELTA);
            ProcessMouseWheelEvent(Delta, static_cast<int32>(ScreenPoint.x),
                                   static_cast<int32>(ScreenPoint.y));
            return 0;
        }
        case WM_INPUT:
        {
            UINT DataSize = 0;
            GetRawInputData(reinterpret_cast<HRAWINPUT>(InLParam), RID_INPUT, nullptr, &DataSize,
                            sizeof(RAWINPUTHEADER));
            if (DataSize > 0)
            {
                BYTE* RawData = new BYTE[DataSize];
                if (GetRawInputData(reinterpret_cast<HRAWINPUT>(InLParam), RID_INPUT, RawData,
                                    &DataSize, sizeof(RAWINPUTHEADER)) == DataSize)
                {
                    RAWINPUT* RawInput = reinterpret_cast<RAWINPUT*>(RawData);
                    if (RawInput->header.dwType == RIM_TYPEMOUSE)
                    {
                        const int32 DeltaX = static_cast<int32>(RawInput->data.mouse.lLastX);
                        const int32 DeltaY = static_cast<int32>(RawInput->data.mouse.lLastY);
                    }
                }
                delete[] RawData;
            }
            return 0;
        }
        case WM_SIZE:
        {
            const int32 Width = LOWORD(InLParam);
            const int32 Height = HIWORD(InLParam);

            Window.SetSize(Width, Height);
            if (MessageHandler != nullptr)
            {
                MessageHandler->OnSizeChanged(Width, Height);
            }
            return 0;
        }
        case WM_DESTROY:
        {
            //
            //extern bool GIsRequestingExit;
            bool GIsRequestingExit;
            GIsRequestingExit = true;
            //
            PostQuitMessage(0);
            return 0;
        }
        default:
            break;
        }

        return DefWindowProcW(InHWnd, InMessage, InWParam, InLParam);
    }

    EKey FWindowsApplication::TranslateKey(WPARAM InWParam) const
    {
        switch (InWParam)
        {
        case 'W':
            return EKey::W;
        case 'A':
            return EKey::A;
        case 'S':
            return EKey::S;
        case 'D':
            return EKey::D;
        case 'Q':
            return EKey::Q;
        case 'E':
            return EKey::E;
        case 'F':
            return EKey::F;
        case 'L':
            return EKey::L;
        case VK_SHIFT:
            return EKey::LeftShift;
        case VK_SPACE:
            return EKey::SpaceBar;
        default:
            return EKey::Invalid;
        }
    }
    EKey FWindowsApplication::TranslateMouseButton(UINT InMessage) const
    {
        switch (InMessage)
        {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
            return EKey::LeftMouseButton;

        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
            return EKey::RightMouseButton;

        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK:
            return EKey::MiddleMouseButton;

        default:
            return EKey::Invalid;
        }
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
