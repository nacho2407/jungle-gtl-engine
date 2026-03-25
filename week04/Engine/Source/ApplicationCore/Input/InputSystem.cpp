#include "Core/CoreMinimal.h"
#include <windowsx.h>
#include "InputSystem.h"

namespace Engine::ApplicationCore
{
    namespace
    {
        static EKey TranslationVirtualKey(WPARAM WParam)
        {
            switch (WParam)
            {
            case 'F':
                return EKey::F;
            case VK_F1:
                return EKey::F1;
            case 'W':
                return EKey::W;
            case 'A':
                return EKey::A;
            case 'S':
                return EKey::S;
            case 'D':
                return EKey::D;
            case 'E':
                return EKey::E;
            case 'Q':
                return EKey::Q;
            case 'O':
                return EKey::O;
            case 'X':
                return EKey::X;
            case VK_SPACE:
                return EKey::Space;
            case VK_DELETE:
                return EKey::Delete;
            case '1':
                return EKey::N1;
            case '2':
                return EKey::N2;
            case '3':
                return EKey::N3;
            default:
                return EKey::Unknown;
            }
        }

        static EKey TranslationMouseButton(WPARAM WParam)
        {
            switch (WParam)
            {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
                return EKey::MouseLeft;

            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
                return EKey::MouseRight;

            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
                return EKey::MouseMiddle;

            default:
                return EKey::Unknown;
            }
        }

        // static bool IsMouseMessage(UINT Msg)
        //{
        //     switch (Msg)
        //     {
        //     case WM_LBUTTONDOWN:
        //     case WM_LBUTTONUP:
        //     case WM_RBUTTONDOWN:
        //     case WM_RBUTTONUP:
        //     case WM_MOUSEMOVE:
        //     case WM_MOUSEWHEEL:
        //         return true;
        //     default:
        //         return false;
        //     }
        // }
    } // namespace

    void FInputSystem::BeginFrame() { State.BeginFrame(); }

    bool FInputSystem::PollEvent(FInputEvent& OutEvent)
    {
        if (EventQueue.empty())
        {
            return false;
        }

        OutEvent = EventQueue.front();
        EventQueue.pop();
        return true;
    }

    void FInputSystem::UpdateModifiers()
    {
        State.Modifiers.bCtrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        State.Modifiers.bShiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        State.Modifiers.bAltDown = (GetKeyState(VK_MENU) & 0x8000) != 0;
    }

    LRESULT FInputSystem::ProcessWin32Message(HWND HWnd, UINT Msg, WPARAM WParam, LPARAM LParam)
    {
        switch (Msg)
        {
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            const EKey Key = TranslationVirtualKey(WParam);
            if (Key == EKey::Unknown)
            {
                break;
            }

            UpdateModifiers();

            FInputEvent Event;
            Event.Type = (Msg == WM_KEYDOWN) ? EInputEventType::KeyDown : EInputEventType::KeyUp;
            Event.Modifiers = State.Modifiers;
            Event.Key = Key;
            Event.bRepeat = (Msg == WM_KEYDOWN) &&
                            ((LParam & (1 << 30)) != 0); // Check the previous key state bit
            Event.Modifiers = State.Modifiers;

            State.KeysDown[static_cast<int32>(Event.Key)] = (Msg == WM_KEYDOWN);
            EventQueue.push(Event);
            break;
        }
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        {
            EKey Key = TranslationMouseButton(Msg);
            if (Key == EKey::Unknown)
            {
                break;
            }

            UpdateModifiers();

            const bool bPressed =
                (Msg == WM_LBUTTONDOWN || Msg == WM_RBUTTONDOWN || Msg == WM_MBUTTONDOWN);

            State.KeysDown[static_cast<int32>(Key)] = bPressed;

            FInputEvent Event;
            Event.Type = bPressed
                             ? EInputEventType::MouseButtonDown
                             : EInputEventType::MouseButtonUp;
            Event.Key = Key;
            Event.MouseX = GET_X_LPARAM(LParam);
            Event.MouseY = GET_Y_LPARAM(LParam);
            Event.Modifiers = State.Modifiers;

            EventQueue.push(Event);
            break;
        }

        case WM_MOUSEMOVE:
        {
            int32 NewX = GET_X_LPARAM(LParam);
            int32 NewY = GET_Y_LPARAM(LParam);

            State.MouseDeltaX += (NewX - State.MouseX);
            State.MouseDeltaY += (NewY - State.MouseY);
            State.MouseX = NewX;
            State.MouseY = NewY;

            UpdateModifiers();

            FInputEvent Event;
            Event.Type = EInputEventType::MouseMove;
            Event.MouseX = NewX;
            Event.MouseY = NewY;
            Event.Modifiers = State.Modifiers;

            EventQueue.push(Event);

            break;
        }
        case WM_MOUSEWHEEL:
        {
            UpdateModifiers();

            const int32 Delta = GET_WHEEL_DELTA_WPARAM(WParam);
            State.WheelDelta += Delta;

            FInputEvent Event;
            Event.Type = EInputEventType::MouseWheel;
            Event.WheelDelta = Delta;
            Event.MouseX = GET_X_LPARAM(LParam);
            Event.MouseY = GET_Y_LPARAM(LParam);
            Event.Modifiers = State.Modifiers;
            EventQueue.push(Event);
            break;
        }

        default:
            break;
        }

        return DefWindowProc(HWnd, Msg, WParam, LParam);
    }
} // namespace Engine::ApplicationCore