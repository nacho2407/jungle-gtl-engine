#include "Engine/Runtime/GameEngine.h"

#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/World.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include "Render/Resource/Texture.h"
#include "Runtime/Script/ScriptManager.h"
#include "Serialization/SceneSaveManager.h"

#include <algorithm>
#include <cctype>
#include <d3d11.h>
#include <filesystem>
#include <fstream>
#include <windows.h>

DEFINE_CLASS(UGameEngine, UEngine)

namespace
{
    FString Trim(const FString& Value)
    {
        const auto IsSpace = [](unsigned char Ch) { return std::isspace(Ch) != 0; };
        auto Begin = std::find_if_not(Value.begin(), Value.end(), IsSpace);
        auto End = std::find_if_not(Value.rbegin(), Value.rend(), IsSpace).base();
        if (Begin >= End)
        {
            return {};
        }
        return FString(Begin, End);
    }

    bool IsCommentOrSection(const FString& Line)
    {
        return Line.empty() || Line[0] == ';' || Line[0] == '#' || Line[0] == '[';
    }

    bool IsMouseButtonVK(int VK)
    {
        return VK == VK_LBUTTON
            || VK == VK_RBUTTON
            || VK == VK_MBUTTON
            || VK == VK_XBUTTON1
            || VK == VK_XBUTTON2;
    }
}

void UGameEngine::Init(FWindowsWindow* InWindow)
{
    const std::filesystem::path LogPath = std::filesystem::path(FPaths::RootDir()) / L"Saves" / L"Logs" / L"Game.log";
    FLog::SetFileOutputPath(LogPath.wstring());
    UE_LOG("[GameEngine] GameClient boot started.");

    UEngine::Init(InWindow);
    FScriptManager::Get().initializeLuaState();
    InitializeRuntimeUIBackend();
    LoadGameSettings();
    LoadStartupWorld();
}

void UGameEngine::Shutdown()
{
    FScriptManager::Get().ShutdownLuaState();
    ShutdownRuntimeUIBackend();
    UEngine::Shutdown();
}

void UGameEngine::BeginPlay()
{
    EnsurePlayerController();
    UEngine::BeginPlay();
    InputSystem::Get().SetUseRawMouse(false);
    MaintainGameInputCapture(InputSystem::Get());
}

void UGameEngine::Tick(float DeltaTime)
{
    UpdateTimeState(DeltaTime);
    InputSystem& Input = InputSystem::Get();
    MaintainGameInputCapture(Input);
    Input.Tick();
    Input.SetGuiMouseCapture(false);
    Input.SetGuiKeyboardCapture(false);
    Input.SetGuiTextInputCapture(false);
    Input.SetGuiViewportMouseBlock(false);

    const bool bRuntimeUIConsumedInput = PumpRuntimeUIInput(Input);
    if (GetRuntimeInputMode() != ERuntimeInputMode::UIOnly && !bRuntimeUIConsumedInput)
    {
        PumpPlayerInput(Input);
    }
    WorldTick(DeltaTime);
    GetAudioSystem().Tick(DeltaTime);
    Render(DeltaTime);
}

void UGameEngine::OnWindowResized(uint32 Width, uint32 Height)
{
    UEngine::OnWindowResized(Width, Height);

    if (PlayerController)
    {
        if (FViewportCamera* RuntimeCamera = PlayerController->GetRuntimeCamera())
        {
            RuntimeCamera->OnResize(Width, Height);
        }
    }

    InputSystem::Get().SetUseRawMouse(false);
}

void UGameEngine::RenderRuntimeUI(const FRuntimeUIRenderContext& Context)
{
    if (!bRuntimeUIBackendInitialized)
    {
        return;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    GetRuntimeUI().Render(RuntimeUIBackend, Context);

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void UGameEngine::LoadGameSettings()
{
    const std::filesystem::path GameIniPath = std::filesystem::path(FPaths::SettingsDir()) / L"Game.ini";
    std::ifstream File(GameIniPath);
    if (!File.is_open())
    {
        UE_LOG("[GameEngine] Settings/Game.ini not found. Using default startup scene: %s",
               StartupSettings.StartupScene.c_str());
        return;
    }

    FString Line;
    while (std::getline(File, Line))
    {
        Line = Trim(Line);
        if (IsCommentOrSection(Line))
        {
            continue;
        }

        const size_t Equals = Line.find('=');
        if (Equals == FString::npos)
        {
            continue;
        }

        const FString Key = Trim(Line.substr(0, Equals));
        const FString Value = Trim(Line.substr(Equals + 1));
        if (Key == "GameName")
        {
            StartupSettings.GameName = Value;
        }
        else if (Key == "StartupScene")
        {
            StartupSettings.StartupScene = Value;
        }
        else if (Key == "PlayerControllerClass")
        {
            StartupSettings.PlayerControllerClass = Value;
        }
    }
}

void UGameEngine::InitializeRuntimeUIBackend()
{
    if (bRuntimeUIBackendInitialized || !Window)
    {
        return;
    }

    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(reinterpret_cast<void*>(Window->GetHWND()));
    ImGui_ImplDX11_Init(
        Renderer.GetFD3DDevice().GetDevice(),
        Renderer.GetFD3DDevice().GetDeviceContext());

    RuntimeUIBackend.SetImageResolver(
        [this](const FString& ImagePath)
        {
            return ResolveRuntimeUIImage(ImagePath);
        });

    bRuntimeUIBackendInitialized = true;
}

void UGameEngine::ShutdownRuntimeUIBackend()
{
    if (!bRuntimeUIBackendInitialized)
    {
        return;
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    bRuntimeUIBackendInitialized = false;
}

FRuntimeUIResolvedImage UGameEngine::ResolveRuntimeUIImage(const FString& ImagePath) const
{
    UTexture* Texture = FResourceManager::Get().LoadTexture(ImagePath);
    if (!Texture || !Texture->GetSRV())
    {
        return {};
    }

    FRuntimeUIResolvedImage Result;
    Result.TextureId = Texture->GetSRV();
    Result.Width = 1.0f;
    Result.Height = 1.0f;

    ID3D11Resource* Resource = nullptr;
    Texture->GetSRV()->GetResource(&Resource);
    if (Resource)
    {
        ID3D11Texture2D* Texture2D = nullptr;
        if (SUCCEEDED(Resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&Texture2D))) && Texture2D)
        {
            D3D11_TEXTURE2D_DESC Desc = {};
            Texture2D->GetDesc(&Desc);
            Result.Width = static_cast<float>(Desc.Width);
            Result.Height = static_cast<float>(Desc.Height);
            Texture2D->Release();
        }
        Resource->Release();
    }

    return Result;
}

void UGameEngine::LoadStartupWorld()
{
    const FString ScenePath = ResolveStartupScenePath();
    if (!ScenePath.empty() && std::filesystem::exists(FPaths::ToWide(ScenePath)))
    {
        FWorldContext LoadedContext;
        FSceneSaveManager::Load(ScenePath, LoadedContext, nullptr);
        if (LoadedContext.World)
        {
            LoadedContext.WorldType = EWorldType::Game;
            LoadedContext.ContextHandle = FName("Game");
            LoadedContext.ContextName = StartupSettings.GameName;
            LoadedContext.World->SetWorldType(EWorldType::Game);
            LoadedContext.World->SyncSpatialIndex();
            WorldList.push_back(LoadedContext);
            SetActiveWorld(LoadedContext.ContextHandle);
            UE_LOG("[GameEngine] Loaded startup scene: %s", ScenePath.c_str());
            return;
        }

        UE_LOG("[GameEngine] Failed to load startup scene: %s", ScenePath.c_str());
    }
    else
    {
        UE_LOG("[GameEngine] Startup scene missing: %s", ScenePath.c_str());
    }

    FWorldContext& Context = CreateWorldContext(EWorldType::Game, FName("Game"), StartupSettings.GameName);
    SetActiveWorld(Context.ContextHandle);
    UE_LOG("[GameEngine] Created empty game world.");
}

void UGameEngine::EnsurePlayerController()
{
    if (PlayerController)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    PlayerController = Cast<APlayerController>(World->SpawnActorByTypeName(StartupSettings.PlayerControllerClass));
    if (!PlayerController)
    {
        UE_LOG("[GameEngine] Failed to spawn PlayerController class: %s", StartupSettings.PlayerControllerClass.c_str());
        return;
    }

    PlayerController->SetFName(FName(StartupSettings.PlayerControllerClass));
    UE_LOG("[GameEngine] Spawned PlayerController class: %s", StartupSettings.PlayerControllerClass.c_str());
    if (FViewportCamera* RuntimeCamera = PlayerController->GetRuntimeCamera())
    {
        RuntimeCamera->OnResize(
            Window ? static_cast<uint32>(Window->GetWidth()) : 1920u,
            Window ? static_cast<uint32>(Window->GetHeight()) : 1080u);
        World->SetActiveCamera(RuntimeCamera);
        UE_LOG("[GameEngine] Runtime camera attached to game world.");
    }
}

void UGameEngine::MaintainGameInputCapture(InputSystem& Input)
{
    if (!Window || !Window->GetHWND())
    {
        return;
    }

    HWND HWnd = Window->GetHWND();
    HWND Foreground = ::GetForegroundWindow();
    if (Foreground != HWnd)
    {
        ::SetForegroundWindow(HWnd);
        ::SetActiveWindow(HWnd);
        ::SetFocus(HWnd);
        return;
    }

    const ERuntimeInputMode InputMode = GetRuntimeInputMode();
    if (InputMode != ERuntimeInputMode::GameOnly || IsRuntimeCursorVisible())
    {
        Input.SetUseRawMouse(false);
        Input.LockMouse(false);
        Input.SetCursorVisibility(true);
        return;
    }

    if (Input.IsUsingRawMouse())
    {
        return;
    }

    RECT ClientRect{};
    if (!::GetClientRect(HWnd, &ClientRect))
    {
        return;
    }

    POINT ClientTopLeft{ ClientRect.left, ClientRect.top };
    ::ClientToScreen(HWnd, &ClientTopLeft);

    const float Width = static_cast<float>(ClientRect.right - ClientRect.left);
    const float Height = static_cast<float>(ClientRect.bottom - ClientRect.top);
    if (Width <= 0.0f || Height <= 0.0f)
    {
        return;
    }

    Input.SetCursorVisibility(false);
    Input.LockMouse(true, static_cast<float>(ClientTopLeft.x), static_cast<float>(ClientTopLeft.y), Width, Height);
    Input.SetUseRawMouse(true);

    if (!bLoggedInputCapture)
    {
        UE_LOG("[GameEngine] Game input captured. Keyboard and raw mouse are routed to PlayerController.");
        bLoggedInputCapture = true;
    }
}

bool UGameEngine::PumpRuntimeUIInput(InputSystem& Input)
{
    if (!Window || !Window->GetHWND())
    {
        return false;
    }

    POINT ClientMousePos = Input.GetMousePos();
    ::ScreenToClient(Window->GetHWND(), &ClientMousePos);

    FRuntimeUIRenderContext Context;
    Context.RenderMode = ERuntimeUIRenderMode::GameClient;
    Context.ViewportMin = FRuntimeUIVector2(0.0f, 0.0f);
    Context.ViewportSize = FRuntimeUIVector2(
        Window ? static_cast<float>(Window->GetWidth()) : 0.0f,
        Window ? static_cast<float>(Window->GetHeight()) : 0.0f);
    Context.DeltaTime = 0.0f;
    GetRuntimeUI().UpdateLayout(Context);

    bool bConsumed = false;
    FRuntimeUIInputEvent Event;
    Event.ScreenPosition = FRuntimeUIVector2(static_cast<float>(ClientMousePos.x), static_cast<float>(ClientMousePos.y));

    if (Input.MouseMoved())
    {
        Event.Type = ERuntimeUIInputEventType::MouseMove;
        Event.MouseButton = ERuntimeUIMouseButton::None;
        bConsumed = GetRuntimeUI().HandleInput(Event) || bConsumed;
    }

    auto PumpMouseButton = [&](int VK, ERuntimeUIMouseButton Button)
    {
        Event.MouseButton = Button;
        if (Input.GetKeyDown(VK))
        {
            Event.Type = ERuntimeUIInputEventType::MouseButtonDown;
            bConsumed = GetRuntimeUI().HandleInput(Event) || bConsumed;
        }
        if (Input.GetKeyUp(VK))
        {
            Event.Type = ERuntimeUIInputEventType::MouseButtonUp;
            bConsumed = GetRuntimeUI().HandleInput(Event) || bConsumed;
        }
    };

    PumpMouseButton(VK_LBUTTON, ERuntimeUIMouseButton::Left);
    PumpMouseButton(VK_RBUTTON, ERuntimeUIMouseButton::Right);
    PumpMouseButton(VK_MBUTTON, ERuntimeUIMouseButton::Middle);

    if (Input.GetScrollDelta() != 0)
    {
        Event.Type = ERuntimeUIInputEventType::MouseWheel;
        Event.MouseButton = ERuntimeUIMouseButton::None;
        Event.WheelDelta = Input.GetScrollNotches();
        bConsumed = GetRuntimeUI().HandleInput(Event) || bConsumed;
    }

    if (bConsumed)
    {
        Input.SetGuiMouseCapture(true);
        Input.SetGuiViewportMouseBlock(true);
    }

    return bConsumed;
}

void UGameEngine::PumpPlayerInput(InputSystem& Input)
{
    if (!PlayerController)
    {
        return;
    }

    for (int VK = 0; VK < 256; ++VK)
    {
        if (IsMouseButtonVK(VK))
        {
            const POINT MousePos = Input.GetMousePos();
            if (Input.GetKeyDown(VK))
            {
                PlayerController->HandleMouseButtonPressed(VK, static_cast<float>(MousePos.x), static_cast<float>(MousePos.y));
            }
            if (Input.GetKey(VK))
            {
                PlayerController->HandleMouseButtonDown(VK, static_cast<float>(Input.MouseDeltaX()), static_cast<float>(Input.MouseDeltaY()));
            }
            if (Input.GetKeyUp(VK))
            {
                PlayerController->HandleMouseButtonReleased(VK, static_cast<float>(MousePos.x), static_cast<float>(MousePos.y));
            }
            continue;
        }

        if (Input.GetKeyDown(VK))
        {
            PlayerController->HandleKeyPressed(VK);
            if (!bLoggedFirstInput)
            {
                UE_LOG("[GameEngine] First key input received: VK=%d", VK);
                bLoggedFirstInput = true;
            }
        }
        if (Input.GetKey(VK))
        {
            PlayerController->HandleKeyDown(VK);
        }
        if (Input.GetKeyUp(VK))
        {
            PlayerController->HandleKeyReleased(VK);
        }
    }

    if (Input.MouseMoved())
    {
        PlayerController->HandleMouseMove(static_cast<float>(Input.MouseDeltaX()), static_cast<float>(Input.MouseDeltaY()));
        if (!bLoggedFirstInput)
        {
            UE_LOG("[GameEngine] First mouse input received: dx=%d dy=%d", Input.MouseDeltaX(), Input.MouseDeltaY());
            bLoggedFirstInput = true;
        }
    }

    if (Input.GetLeftDragging())
    {
        PlayerController->HandleMouseDrag(VK_LBUTTON, static_cast<float>(Input.MouseDeltaX()), static_cast<float>(Input.MouseDeltaY()));
    }
    if (Input.GetLeftDragEnd())
    {
        const POINT MousePos = Input.GetMousePos();
        PlayerController->HandleMouseDragEnd(VK_LBUTTON, static_cast<float>(MousePos.x), static_cast<float>(MousePos.y));
    }
    if (Input.GetMiddleDragging())
    {
        PlayerController->HandleMouseDrag(VK_MBUTTON, static_cast<float>(Input.MouseDeltaX()), static_cast<float>(Input.MouseDeltaY()));
    }
    if (Input.GetMiddleDragEnd())
    {
        const POINT MousePos = Input.GetMousePos();
        PlayerController->HandleMouseDragEnd(VK_MBUTTON, static_cast<float>(MousePos.x), static_cast<float>(MousePos.y));
    }
    if (Input.GetRightDragging())
    {
        PlayerController->HandleMouseDrag(VK_RBUTTON, static_cast<float>(Input.MouseDeltaX()), static_cast<float>(Input.MouseDeltaY()));
    }
    if (Input.GetRightDragEnd())
    {
        const POINT MousePos = Input.GetMousePos();
        PlayerController->HandleMouseDragEnd(VK_RBUTTON, static_cast<float>(MousePos.x), static_cast<float>(MousePos.y));
    }
    if (Input.GetScrollDelta() != 0)
    {
        PlayerController->HandleMouseWheel(Input.GetScrollNotches());
    }
}

FString UGameEngine::ResolveStartupScenePath() const
{
    if (StartupSettings.StartupScene.empty())
    {
        return {};
    }

    return FPaths::Normalize(FPaths::ToAbsoluteString(FPaths::ToWide(StartupSettings.StartupScene)));
}
