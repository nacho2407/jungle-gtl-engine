#include "EditorEngineLoop.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "imgui.h"
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND HWnd, UINT Message,
                                                             WPARAM WParam, LPARAM LParam);

bool FEditorEngineLoop::PreInit(HINSTANCE HInstance, uint32 NCmdShow)
{
    (void)HInstance;
    (void)NCmdShow;

    InputSystem = new Engine::ApplicationCore::FInputSystem();

#if defined(_WIN32)
    Application = Engine::ApplicationCore::FWindowsApplication::Create();
    if (Application == nullptr)
    {
        return false;
    }

    Engine::ApplicationCore::FWindowsApplication* WindowsApplication = GetWindowsApplication();
    if (WindowsApplication == nullptr)
    {
        return false;
    }

    if (!WindowsApplication->CreateEditorWindow(L"Jungle Editor", 1920, 1080))
    {
        return false;
    }

    Application->SetInputSystem(InputSystem);
#else
    return false;
#endif

    Editor = new FEditor();
    Editor->SetChromeHost(this);
    Editor->Create();
    Editor->Initialize();

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

    ImGui::CreateContext();
    ImGui_ImplWin32_Init((void*)WindowHandle);
    ImGui_ImplDX11_Init(Renderer->GetRHI().GetDevice(), Renderer->GetRHI().GetDeviceContext());

    if (Engine::ApplicationCore::FWindowsApplication* WindowsApplication = GetWindowsApplication())
    {
        WindowsApplication->SetMessageHandler(&FEditorEngineLoop::HandleEditorMessage, this);
    }

    CachedWindowWidth = Application->GetWindowWidth();
    CachedWindowHeight = Application->GetWindowHeight();
    Editor->OnWindowResized(static_cast<float>(CachedWindowWidth),
                            static_cast<float>(CachedWindowHeight));

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
    if (Engine::ApplicationCore::FWindowsApplication* WindowsApplication = GetWindowsApplication())
    {
        WindowsApplication->SetMessageHandler(nullptr, nullptr);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (Renderer != nullptr)
    {
        Renderer->ShutdownModule();
        delete Renderer;
        Renderer = nullptr;
    }

    if (Editor != nullptr)
    {
        Editor->Release();
        delete Editor;
        Editor = nullptr;
    }

    if (Application != nullptr)
    {
        Application->DestroyApplicationWindow();
        delete Application;
        Application = nullptr;
    }

    delete InputSystem;
    InputSystem = nullptr;
}

void FEditorEngineLoop::Tick()
{
    Application->PumpMessages();
    if (Application->IsExitRequested())
    {
        bIsExit = true;
        return;
    }

    RunFrameOnce();

    FPlatformTime::Sleep(0.0f);
}

void FEditorEngineLoop::InitializeForTime()
{
    PrevTime = FPlatformTime::Seconds();
    DeltaTime = 0.0f;
}

void FEditorEngineLoop::SetTitleBarMetrics(
    int32 Height, const TArray<FEditorChromeRect>& InteractiveRects)
{
    Engine::ApplicationCore::FWindowsApplication* WindowsApplication = GetWindowsApplication();
    if (WindowsApplication == nullptr)
    {
        return;
    }

    Engine::ApplicationCore::FCustomTitleBarState TitleBarState;
    TitleBarState.TitleBarHeight = Height;
    TitleBarState.InteractiveRects.reserve(InteractiveRects.size());

    for (const FEditorChromeRect& Rect : InteractiveRects)
    {
        Engine::ApplicationCore::FWindowHitTestRect WindowRect;
        WindowRect.Left = Rect.Left;
        WindowRect.Top = Rect.Top;
        WindowRect.Right = Rect.Right;
        WindowRect.Bottom = Rect.Bottom;
        TitleBarState.InteractiveRects.push_back(WindowRect);
    }

    WindowsApplication->SetCustomTitleBarState(TitleBarState);
}

void FEditorEngineLoop::MinimizeWindow()
{
    if (Engine::ApplicationCore::FWindowsApplication* WindowsApplication = GetWindowsApplication())
    {
        WindowsApplication->MinimizeWindow();
    }
}

void FEditorEngineLoop::ToggleMaximizeWindow()
{
    if (Engine::ApplicationCore::FWindowsApplication* WindowsApplication = GetWindowsApplication())
    {
        WindowsApplication->ToggleMaximizeWindow();
    }
}

void FEditorEngineLoop::CloseWindow()
{
    if (Engine::ApplicationCore::FWindowsApplication* WindowsApplication = GetWindowsApplication())
    {
        WindowsApplication->CloseWindow();
    }
}

bool FEditorEngineLoop::IsWindowMaximized() const
{
    if (Engine::ApplicationCore::FWindowsApplication* WindowsApplication = GetWindowsApplication())
    {
        return WindowsApplication->IsWindowMaximized();
    }

    return false;
}

const wchar_t* FEditorEngineLoop::GetWindowTitle() const
{
    if (Engine::ApplicationCore::FWindowsApplication* WindowsApplication = GetWindowsApplication())
    {
        return WindowsApplication->GetWindowTitle();
    }

    return L"";
}

bool FEditorEngineLoop::HandleEditorMessage(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam,
                                            LRESULT& OutResult, void* UserData)
{
    FEditorEngineLoop* EditorEngineLoop = static_cast<FEditorEngineLoop*>(UserData);
    if (EditorEngineLoop == nullptr)
    {
        return false;
    }

    return EditorEngineLoop->HandleEditorMessageInternal(HWnd, Message, WParam, LParam,
                                                         OutResult);
}

bool FEditorEngineLoop::HandleEditorMessageInternal(HWND HWnd, UINT Message, WPARAM WParam,
                                                    LPARAM LParam, LRESULT& OutResult)
{
    switch (Message)
    {
    case WM_ENTERSIZEMOVE:
        bIsInSizeMoveLoop = true;
        if (Renderer != nullptr)
        {
            bSavedVSyncEnabled = Renderer->IsVSyncEnabled();
            Renderer->SetVSyncEnabled(false);
        }
        OutResult = 0;
        return true;

    case WM_EXITSIZEMOVE:
        if (bIsInSizeMoveLoop)
        {
            HandleWindowResize();
            RunFrameOnceWithoutResize();
            if (Renderer != nullptr)
            {
                Renderer->SetVSyncEnabled(bSavedVSyncEnabled);
            }
            bIsInSizeMoveLoop = false;
        }
        OutResult = 0;
        return false;

    case WM_SIZE:
        if (bIsInSizeMoveLoop && WParam != SIZE_MINIMIZED)
        {
            HandleWindowResize();
            RunFrameOnceWithoutResize();
        }
        break;

    case WM_PAINT:
        if (bIsInSizeMoveLoop)
        {
            PAINTSTRUCT PaintStruct = {};
            BeginPaint(HWnd, &PaintStruct);
            EndPaint(HWnd, &PaintStruct);

            RunFrameOnceWithoutResize();

            OutResult = 0;
            return true;
        }
        break;

    case WM_ERASEBKGND:
        if (bIsInSizeMoveLoop)
        {
            OutResult = 1;
            return true;
        }
        break;

    default:
        break;
    }

    const LRESULT ImGuiResult = ImGui_ImplWin32_WndProcHandler(HWnd, Message, WParam, LParam);
    if (ImGuiResult != 0)
    {
        OutResult = ImGuiResult;
        return true;
    }

    if (ImGui::GetCurrentContext() == nullptr)
    {
        return false;
    }

    const ImGuiIO& IO = ImGui::GetIO();
    switch (Message)
    {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_MOUSEMOVE:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        if (IO.WantCaptureMouse)
        {
            OutResult = 0;
            return true;
        }
        break;

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_CHAR:
        if (IO.WantCaptureKeyboard || IO.WantTextInput)
        {
            OutResult = 0;
            return true;
        }
        break;

    default:
        break;
    }

    return false;
}

bool FEditorEngineLoop::HandleWindowResize()
{
    const int32 CurrentWindowWidth = Application->GetWindowWidth();
    const int32 CurrentWindowHeight = Application->GetWindowHeight();
    if (CurrentWindowWidth == CachedWindowWidth && CurrentWindowHeight == CachedWindowHeight)
    {
        return false;
    }

    CachedWindowWidth = CurrentWindowWidth;
    CachedWindowHeight = CurrentWindowHeight;

    if (Renderer != nullptr)
    {
        Renderer->OnWindowResized(CurrentWindowWidth, CurrentWindowHeight);
    }

    if (Editor != nullptr)
    {
        Editor->OnWindowResized(static_cast<float>(CurrentWindowWidth),
                                static_cast<float>(CurrentWindowHeight));
    }

    return true;
}

bool FEditorEngineLoop::RunFrameOnce()
{
    HandleWindowResize();
    return RunFrameOnceWithoutResize();
}

bool FEditorEngineLoop::RunFrameOnceWithoutResize()
{
    if (bIsRenderingDuringSizeMove)
    {
        return false;
    }

    if (Application == nullptr || Editor == nullptr || Renderer == nullptr || InputSystem == nullptr)
    {
        return false;
    }

    if (Application->IsExitRequested())
    {
        bIsExit = true;
        return false;
    }

    bIsRenderingDuringSizeMove = true;

    InputSystem->BeginFrame();
    UpdateFrameTiming();

    Editor->SetMainLoopFPS(MainLoopFPS);
    Editor->Tick(DeltaTime, InputSystem);

    Renderer->BeginFrame();
    Renderer->Render(Editor->GetEditorRenderData(), Editor->GetSceneRenderData());
    Editor->DrawPanel();
    Renderer->EndFrame();

    bIsRenderingDuringSizeMove = false;
    return true;
}

void FEditorEngineLoop::UpdateFrameTiming()
{
    const float CurrentTime = FPlatformTime::Seconds();
    DeltaTime = CurrentTime - PrevTime;
    PrevTime = CurrentTime;
    MainLoopFPS = (DeltaTime > 0.0f) ? (1.0f / DeltaTime) : 0.0f;
}

Engine::ApplicationCore::FWindowsApplication* FEditorEngineLoop::GetWindowsApplication() const
{
#if defined(_WIN32)
    return static_cast<Engine::ApplicationCore::FWindowsApplication*>(Application);
#else
    return nullptr;
#endif
}
