#include "EditorEngineLoop.h"
#include "EditorEngineLoop.h"

#include <wrl/client.h>

#include "imgui.h"
#include "imgui_internal.h"

bool FEditorEngineLoop::PreInit(HINSTANCE HInstance, uint32 NCmdShow)
{
    (void)HInstance;
    (void)NCmdShow;

    InputSystem = new Engine::ApplicationCore::FInputSystem();

    /* Application Setting */
#if defined(_WIN32)
    Application = Engine::ApplicationCore::FWindowsApplication::Create();
    if (Application == nullptr)
    {
        return false;
    }

    if (!Application->CreateApplicationWindow(L"JungleWindowClass", 1920, 1080))
    {
        return false;
    }

    Application->SetInputSystem(InputSystem);
#else

#endif

    /* Editor Initialize */
    Editor = new FEditor();
    Editor->Create();
    Editor->Initialize();

    /* Renderer Initialize */
    Renderer = new FRendererModule();
    if (Renderer == nullptr)
    {
        return false;
    }

    HWND WindowHandle = static_cast<HWND>(Application->GetNativeWindowHandle());
    if (WindowHandle == nullptr)
    {
        return false;
    }

    if (!Renderer->StartupModule(WindowHandle))
    {
        return false;
    }

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
    if (Renderer != nullptr)
    {
        Renderer->ShutdownModule();
        delete Renderer;
        Renderer = nullptr;
    }

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

    /* Editor Viewport Client */

    /* Render */
    if (Renderer != nullptr)
    {
        Renderer->BeginFrame();
        Renderer->Render(Editor->GetEditorRenderData(), Editor->GetSceneRenderData());
        Renderer->EndFrame();
    }

    FPlatformTime::Sleep(0.f);
}

void FEditorEngineLoop::InitializeForTime()
{
    PrevTime = FPlatformTime::Seconds();
    DeltaTime = 0.0f;
}
