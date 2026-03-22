#include "EditorEngineLoop.h"

#include <wrl/client.h>

#include "imgui.h"
#include "imgui_internal.h"

//extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, uint32 msg, WPARAM wParam, LPARAM lParam);

// LRESULT FEditorEngineLoop::StaticWndProc(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam)
// {
//     FEditorEngineLoop *EditorEngineLoop = reinterpret_cast<FEditorEngineLoop *>(GetWindowLongPtr(
//         HWnd, GWLP_USERDATA));
//
//     if (Message == WM_NCCREATE)
//     {
//         CREATESTRUCTW *CreateStruct = reinterpret_cast<CREATESTRUCTW *>(LParam);
//         EditorEngineLoop = reinterpret_cast<FEditorEngineLoop *>(CreateStruct->lpCreateParams);
//         SetWindowLongPtr(HWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(EditorEngineLoop));
//     }
//
//     if (EditorEngineLoop)
//     {
//         return EditorEngineLoop->WndProc(HWnd, Message, WParam, LParam);
//     }
//
//     return DefWindowProc(HWnd, Message, WParam, LParam);
// }
//
// LRESULT FEditorEngineLoop::WndProc(HWND HWnd, uint32 Message, WPARAM WParam, LPARAM LParam)
// {
//     if (ImGui_ImplWin32_WndProcHandler(HWnd, Message, WParam, LParam))
//     {
//         return 1;
//     }
//
//     switch (Message)
//     {
//     case WM_DESTROY:
//
//         bIsExit = true;
//         PostQuitMessage(0);
//         return 0;
//     case WM_SIZE:
//         if (Editor)
//         {
//             Editor->OnWindowResized(LOWORD(LParam), HIWORD(LParam));    
//         }
//         break;
//     case WM_SIZING:
//         //  Render for Re-Sizing
//         break;
//     default:
//         break;
//     }
//
//     return DefWindowProc(HWnd, Message, WParam, LParam);
// }

bool FEditorEngineLoop::PreInit(HINSTANCE HInstance, uint32 NCmdShow)
{
    (void)NCmdShow;

    InputSystem = new Engine::ApplicationCore::FInputSystem();

    /* Application Setting */
#if defined(_WIN32)
    Application = Engine::ApplicationCore::FWindowsApplication::Create();
    Application->CreateApplicationWindow(L"JungleWindowClass", 1920, 1080);
    Application->SetInputSystem(InputSystem);
#else

#endif

    /* Editor Initialize */
    Editor = new FEditor();
    Editor->Create();
    Editor->Initialize();

    InitializeForTime();
    return true;
}

int32 FEditorEngineLoop::Run()
{
    while (!bIsExit)
    {
        if (bIsExit)
        {
            break;
        }

        Tick();
    }

    return 0;
}

void FEditorEngineLoop::ShutDown()
{
    Editor->Release();
    delete Editor;
    Editor = nullptr;

    Application->DestroyApplicationWindow();
    delete Application;
    Application = nullptr;
   
    delete InputSystem;
    InputSystem = nullptr;

    //  TODO : Garbage Sweep
}

void FEditorEngineLoop::Tick()
{
    /* Application Pump Message */
    Application->PumpMessages();
    if (Application->IsExitRequested())
    {
        bIsExit = true;
        return;
    }

    InputSystem->BeginFrame();

    
    /* Time Measuring */
    DeltaTime = FPlatformTime::Seconds() - PrevTime;
    PrevTime = FPlatformTime::Seconds();

    MainLoopFPS = 1.0f / DeltaTime;

    /* Engine Tick */
    //  Engine->Tick(DeltaTime);
    /* Editor Update */
    Editor->Tick(DeltaTime, InputSystem);

    /* Rendering Prepare Stage */

    /* Editor Viewport Client */

    /* Render End Stage */

    FPlatformTime::Sleep(0.f);
}

void FEditorEngineLoop::InitializeForTime()
{
    PrevTime = FPlatformTime::Seconds();
    DeltaTime = 0.0f;
}
