#include <Core/CoreMinimal.h>
#include <windowsx.h>
#include "InputSystem.h"



namespace Engine::ApplicationCore
{
    // LRESULT FInputSystem::StaticWndProc(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam)
    // {
    //     FInputSystem *EditorEngineLoop = reinterpret_cast<FInputSystem *>(GetWindowLongPtr(
    //         HWnd, GWLP_USERDATA));
    //
    //     if (Message == WM_NCCREATE)
    //     {
    //         CREATESTRUCTW *CreateStruct = reinterpret_cast<CREATESTRUCTW *>(LParam);
    //         EditorEngineLoop = reinterpret_cast<FInputSystem *>(CreateStruct->lpCreateParams);
    //         SetWindowLongPtr(HWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(EditorEngineLoop));
    //     }
    //
    //     if (EditorEngineLoop)
    //     {
    //         return EditorEngineLoop->WndProc(Message, WParam, LParam);
    //     }
    //
    //     return DefWindowProc(HWnd, Message, WParam, LParam);
    // }
    
    void FInputSystem::BeginFrame()
    {
        State.BeginFrame();
    }

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
        State.Modifiers.bCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        State.Modifiers.bShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        State.Modifiers.bAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    }

    LRESULT FInputSystem::ProcessWin32Message(HWND HWnd,UINT Msg, WPARAM WParam, LPARAM LParam)
    {
        switch (Msg)
        {
        case WM_KEYDOWN:
        {
            UpdateModifiers();

            FInputEvent E;
            E.Type = EInputEventType::KeyDown;
            E.bRepeat = (LParam & 0x40000000) != 0;
            E.Modifiers = State.Modifiers;

            if (WParam == 'W')
                E.Key = EKey::W;
            else if (WParam == 'E')
                E.Key = EKey::E;
            else if (WParam == 'R')
                E.Key = EKey::R;
            else if (WParam == 'F')
                E.Key = EKey::F;
            else if (WParam == VK_DELETE)
                E.Key = EKey::Delete;

            State.KeysDown[static_cast<int32>(E.Key)] = true;
            EventQueue.push(E);
            break;
        }
        case WM_KEYUP:
        {
            UpdateModifiers();

            FInputEvent E;
            E.Type = EInputEventType::KeyUp;
            E.Modifiers = State.Modifiers;

            if (WParam == 'W')
                E.Key = EKey::W;
            else if (WParam == 'E')
                E.Key = EKey::E;
            else if (WParam == 'R')
                E.Key = EKey::R;
            else if (WParam == 'F')
                E.Key = EKey::F;
            else if (WParam == VK_DELETE)
                E.Key = EKey::Delete;

            State.KeysDown[static_cast<int32>(E.Key)] = false;
            EventQueue.push(E);
            break;
        }
        case WM_LBUTTONDOWN:
        {
            State.KeysDown[static_cast<int32>(EKey::MouseLeft)] = true;

            FInputEvent E;
            E.Type = EInputEventType::MouseButtonDown;
            E.Key = EKey::MouseLeft;
            E.MouseX = GET_X_LPARAM(LParam);
            E.MouseY = GET_Y_LPARAM(LParam);
            EventQueue.push(E);
            break;
        }
        case WM_LBUTTONUP:
        {
            State.KeysDown[static_cast<int32>(EKey::MouseLeft)] = false;

            FInputEvent E;
            E.Type = EInputEventType::MouseButtonUp;
            E.Key = EKey::MouseLeft;
            E.MouseX = GET_X_LPARAM(LParam);
            E.MouseY = GET_Y_LPARAM(LParam);
            EventQueue.push(E);
            break;
        }
        case WM_RBUTTONDOWN:
            State.KeysDown[static_cast<int32>(EKey::MouseRight)] = true;
            break;

        case WM_RBUTTONUP:
            State.KeysDown[static_cast<int32>(EKey::MouseRight)] = false;
            break;

        case WM_MOUSEMOVE:
        {
            int32 NewX = GET_X_LPARAM(LParam);
            int32 NewY = GET_Y_LPARAM(LParam);

            State.MouseDeltaX += (NewX - State.MouseX);
            State.MouseDeltaY += (NewY - State.MouseY);
            State.MouseX = NewX;
            State.MouseY = NewY;
            break;
        }
        case WM_MOUSEWHEEL:
        {
            int32 Delta = GET_WHEEL_DELTA_WPARAM(WParam);
            State.WheelDelta += Delta;

            FInputEvent E;
            E.Type = EInputEventType::MouseWheel;
            E.WheelDelta = Delta;
            EventQueue.push(E);
            break;
        }
        case WM_SIZE:
        {
            FInputEvent E;
            E.Type = EInputEventType::WindowResize;
            E.Width = LOWORD(LParam);
            E.Height = HIWORD(LParam);
            EventQueue.push(E);
            break;
        }
        default:
            break;
        }

        return DefWindowProc(HWnd, Msg, WParam, LParam);
    }
} // namespace Engine::ApplicationCore
