#include "EditorEngineLoop.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include "imgui.h"
#include "imgui_internal.h"
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <cstring>

#include "Core/Misc/NameSubsystem.h"
#include "Asset/AssetManager.h"
#include "Asset/AssetLoader/FontAtlasLoader.h"
#include "Asset/AssetLoader/SubUVAtlasLoader.h"
#include "Asset/AssetLoader/TextureLoader.h"
#include "Asset/AssetLoader/StaticMeshLoader.h"
#include "Asset/AssetLoader/MaterialLoader.h" 

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND HWnd, UINT Message,
                                                             WPARAM WParam, LPARAM LParam);

namespace
{
    ImVec4 MakeColor(uint8 R, uint8 G, uint8 B, uint8 A = 255)
    {
        return ImVec4(static_cast<float>(R) / 255.0f, static_cast<float>(G) / 255.0f,
                      static_cast<float>(B) / 255.0f, static_cast<float>(A) / 255.0f);
    }

    ImVec4 WithAlpha(const ImVec4& Color, float Alpha)
    {
        return ImVec4(Color.x, Color.y, Color.z, Alpha);
    }

    ImVec4 LerpColor(const ImVec4& A, const ImVec4& B, float T)
    {
        return ImVec4(A.x + (B.x - A.x) * T, A.y + (B.y - A.y) * T, A.z + (B.z - A.z) * T,
                      A.w + (B.w - A.w) * T);
    }

    void ApplyCoPassImGuiStyle()
    {
        ImGui::StyleColorsDark();

        ImGuiStyle& Style = ImGui::GetStyle();
        ImVec4*     Colors = Style.Colors;

        const ImVec4 PanelBg = MakeColor(37, 37, 38);
        const ImVec4 RaisedBg = MakeColor(45, 45, 48);
        const ImVec4 RaisedBgSoft = MakeColor(52, 52, 56);
        const ImVec4 BrandBlue = MakeColor(41, 103, 255);
        const ImVec4 BrandBlueSoft = LerpColor(BrandBlue, MakeColor(255, 255, 255), 0.18f);
        const ImVec4 BrandBlueDeep = LerpColor(BrandBlue, MakeColor(0, 0, 0), 0.18f);
        const ImVec4 BrandBlueDark = LerpColor(BrandBlue, MakeColor(0, 0, 0), 0.38f);

        Colors[ImGuiCol_WindowBg] = PanelBg;
        Colors[ImGuiCol_ChildBg] = PanelBg;
        Colors[ImGuiCol_PopupBg] = RaisedBg;
        Colors[ImGuiCol_MenuBarBg] = RaisedBg;
        Colors[ImGuiCol_TitleBg] = PanelBg;
        Colors[ImGuiCol_TitleBgActive] = PanelBg;
        Colors[ImGuiCol_TitleBgCollapsed] = WithAlpha(PanelBg, 0.92f);
        Colors[ImGuiCol_DockingEmptyBg] = PanelBg;
        Colors[ImGuiCol_TableHeaderBg] = RaisedBgSoft;

        Colors[ImGuiCol_FrameBg] = WithAlpha(BrandBlueDeep, 0.22f);
        Colors[ImGuiCol_FrameBgHovered] = WithAlpha(BrandBlue, 0.32f);
        Colors[ImGuiCol_FrameBgActive] = WithAlpha(BrandBlueSoft, 0.40f);

        Colors[ImGuiCol_CheckMark] = BrandBlue;
        Colors[ImGuiCol_SliderGrab] = BrandBlueDeep;
        Colors[ImGuiCol_SliderGrabActive] = BrandBlue;

        Colors[ImGuiCol_Button] = WithAlpha(BrandBlue, 0.38f);
        Colors[ImGuiCol_ButtonHovered] = WithAlpha(BrandBlueSoft, 0.88f);
        Colors[ImGuiCol_ButtonActive] = BrandBlue;

        Colors[ImGuiCol_Header] = WithAlpha(BrandBlue, 0.28f);
        Colors[ImGuiCol_HeaderHovered] = WithAlpha(BrandBlueSoft, 0.72f);
        Colors[ImGuiCol_HeaderActive] = WithAlpha(BrandBlue, 0.92f);

        Colors[ImGuiCol_SeparatorHovered] = WithAlpha(BrandBlue, 0.78f);
        Colors[ImGuiCol_SeparatorActive] = BrandBlue;

        Colors[ImGuiCol_ResizeGrip] = WithAlpha(BrandBlue, 0.18f);
        Colors[ImGuiCol_ResizeGripHovered] = WithAlpha(BrandBlueSoft, 0.62f);
        Colors[ImGuiCol_ResizeGripActive] = WithAlpha(BrandBlue, 0.92f);

        Colors[ImGuiCol_TabHovered] = WithAlpha(BrandBlueSoft, 0.80f);
        Colors[ImGuiCol_Tab] = LerpColor(RaisedBg, BrandBlueDark, 0.30f);
        Colors[ImGuiCol_TabSelected] = LerpColor(RaisedBg, BrandBlue, 0.20f);
        Colors[ImGuiCol_TabSelectedOverline] = BrandBlue;
        Colors[ImGuiCol_TabDimmedSelected] =
            LerpColor(PanelBg, BrandBlueDark, 0.22f);
        Colors[ImGuiCol_TabDimmedSelectedOverline] = WithAlpha(BrandBlue, 0.55f);

        Colors[ImGuiCol_DockingPreview] = WithAlpha(BrandBlue, 0.65f);
        Colors[ImGuiCol_PlotLinesHovered] = BrandBlueSoft;
        Colors[ImGuiCol_TextSelectedBg] = WithAlpha(BrandBlue, 0.30f);
        Colors[ImGuiCol_DragDropTarget] = BrandBlue;
        Colors[ImGuiCol_NavCursor] = BrandBlue;
    }
} // namespace

bool FEditorEngineLoop::PreInit(HINSTANCE HInstance, uint32 NCmdShow)
{
    (void)HInstance;
    (void)NCmdShow;

    Engine::Core::Misc::FNameSubsystem::Init();

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

    if (!WindowsApplication->CreateEditorWindow(L"CO-PASS", 1920, 1080))
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

    AssetManager = new UAssetManager();
    TextureAssetLoader = new FTextureLoader(&Renderer->GetRHI());
    FontAssetLoader = new FFontAtlasLoader(&Renderer->GetRHI());
    SubUVAtlasAssetLoader = new FSubUVAtlasLoader(&Renderer->GetRHI());
    StaticMeshLoader = new FStaticMeshLoader(&Renderer->GetRHI(), AssetManager);
    MaterialAssetLoader = new FMaterialLoader(AssetManager);
    AssetManager->RegisterLoader(TextureAssetLoader);
    AssetManager->RegisterLoader(FontAssetLoader);
    AssetManager->RegisterLoader(SubUVAtlasAssetLoader);
    AssetManager->RegisterLoader(StaticMeshLoader);
    AssetManager->RegisterLoader(MaterialAssetLoader);
    Editor->SetRuntimeServices(&Renderer->GetRHI(), AssetManager);

    ImGui::CreateContext();
    ApplyCoPassImGuiStyle();
    ImGuiIO& IO = ImGui::GetIO();
#ifdef IMGUI_HAS_DOCK
    // 도킹 지원 ImGui를 교체한 뒤에는 여기서 기능 플래그를 켜야 DockSpace API가 실제로 동작합니다.
    IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif
    if (ImFont* KoreanFont = IO.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf",
                                                          18.0f, nullptr,
                                                          IO.Fonts->GetGlyphRangesKorean()))
    {
        IO.FontDefault = KoreanFont;
    }
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
    Editor->GetViewportClient().OnPickRequested = [this](int32 X, int32 Y) -> FPickResult
    {
        FPickResult Result;
        // EngineLoop는 Renderer와 Editor 모두에 접근 가능하므로 픽킹을 직접 수행해서 반환
        Renderer->Pick(Editor->GetEditorRenderData(), X, Y, Result);
        return Result;
    };
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

    if (Editor != nullptr)
    {
        Editor->SetRuntimeServices(nullptr, nullptr);
    }

     delete StaticMeshLoader;
    StaticMeshLoader = nullptr;

     delete MaterialAssetLoader;
    MaterialAssetLoader = nullptr;

    delete FontAssetLoader;
    FontAssetLoader = nullptr;

    delete SubUVAtlasAssetLoader;
    SubUVAtlasAssetLoader = nullptr;

    delete TextureAssetLoader;
    TextureAssetLoader = nullptr;

    delete AssetManager;
    AssetManager = nullptr;

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

    // ImGui에서 계산한 버튼 위치를 Win32 client 좌표 기준으로 다시 전달합니다.
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

void* FEditorEngineLoop::GetNativeWindowHandle() const
{
    return Application != nullptr ? Application->GetNativeWindowHandle() : nullptr;
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
    case WM_CLOSE:
        if (Editor != nullptr && Editor->RequestCloseEditor())
        {
            if (Engine::ApplicationCore::FWindowsApplication* WindowsApplication =
                    GetWindowsApplication())
            {
                WindowsApplication->DestroyApplicationWindow();
            }
        }

        OutResult = 0;
        return true;

    case WM_NCHITTEST:
        if (ImGui::GetCurrentContext() != nullptr)
        {
            POINT ScreenPosition = { GET_X_LPARAM(LParam), GET_Y_LPARAM(LParam) };
            POINT ClientPosition = ScreenPosition;
            ScreenToClient(HWnd, &ClientPosition);

            ImGuiWindow* HoveredWindow = nullptr;
            ImGuiWindow* HoveredWindowUnderMovingWindow = nullptr;
            ImGui::FindHoveredWindowEx(
                ImVec2(static_cast<float>(ClientPosition.x), static_cast<float>(ClientPosition.y)),
                false, &HoveredWindow, &HoveredWindowUnderMovingWindow);

            if (HoveredWindow != nullptr)
            {
                ImGuiWindow* RootWindow = HoveredWindow->RootWindowDockTree != nullptr
                                              ? HoveredWindow->RootWindowDockTree
                                              : HoveredWindow;

                // 커스텀 타이틀바는 native caption hit-test를 유지하고,
                // 그 위에 겹쳐진 floating ImGui 창만 client로 돌려 ImGui 드래그를 살립니다.
                if ((RootWindow->Flags & ImGuiWindowFlags_NoInputs) == 0 &&
                    std::strcmp(RootWindow->Name, "##EditorChrome") != 0)
                {
                    OutResult = HTCLIENT;
                    return true;
                }
            }
        }
        break;

    case WM_ENTERSIZEMOVE:
        // Win32 sizing loop가 시작되면 메인 루프 대신 WndProc 안에서 즉시 프레임을 돌립니다.
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
            // sizing loop 종료 직전에 최종 크기와 마지막 프레임을 한 번 더 맞춘 뒤 VSync를 복구합니다.
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
            // 드래그 중 전달되는 중간 크기마다 back buffer와 viewport를 즉시 갱신합니다.
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

            // modal loop 안에서 발생하는 paint도 직접 소비해서 화면이 멈춘 것처럼 보이지 않게 합니다.
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
            // 타이틀바 버튼이나 패널이 마우스를 잡은 동안에는 에디터 입력 라우터로 넘기지 않습니다.
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
            // 텍스트 입력 중에는 카메라 단축키 같은 엔진 입력이 섞이지 않게 막습니다.
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
        // 렌더 타깃 크기를 먼저 맞추고
        Renderer->OnWindowResized(CurrentWindowWidth, CurrentWindowHeight);
    }

    if (Editor != nullptr)
    {
        // Editor 쪽에서는 SceneView와 카메라 투영행렬까지 이어서 갱신합니다.
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
        // resize 중 WM_SIZE/WM_PAINT가 중첩으로 들어와도 프레임 실행은 한 번만 허용합니다.
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
    const double CurrentTime = FPlatformTime::Seconds();
    double RawDeltaTime = CurrentTime - PrevTime;
    PrevTime = CurrentTime;

    if (RawDeltaTime < (1.0 / 1000.0))
    {
        RawDeltaTime = 1.0 / 1000.0;
    }
    else if (RawDeltaTime > (1.0 / 15.0))
    {
        RawDeltaTime = 1.0 / 15.0;
    }

    DeltaTime = static_cast<float>(RawDeltaTime);
    MainLoopFPS = static_cast<float>(1.0 / RawDeltaTime);
}

Engine::ApplicationCore::FWindowsApplication* FEditorEngineLoop::GetWindowsApplication() const
{
#if defined(_WIN32)
    return static_cast<Engine::ApplicationCore::FWindowsApplication*>(Application);
#else
    return nullptr;
#endif
}
