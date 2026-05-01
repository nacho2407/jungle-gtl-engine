#include "Engine/Runtime/GameEngine.h"

#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/World.h"
#include "Serialization/SceneSaveManager.h"

#include <algorithm>
#include <cctype>
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
}

void UGameEngine::Init(FWindowsWindow* InWindow)
{
    const std::filesystem::path LogPath = std::filesystem::path(FPaths::RootDir()) / L"Saves" / L"Logs" / L"Game.log";
    FLog::SetFileOutputPath(LogPath.wstring());
    UE_LOG("[GameEngine] GameClient boot started.");

    UEngine::Init(InWindow);
    LoadGameSettings();
    LoadStartupWorld();
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
    InputSystem& Input = InputSystem::Get();
    MaintainGameInputCapture(Input);
    Input.Tick();

    PumpPlayerInput(Input);
    WorldTick(DeltaTime);
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

void UGameEngine::PumpPlayerInput(InputSystem& Input)
{
    if (!PlayerController)
    {
        return;
    }

    for (int VK = 0; VK < 256; ++VK)
    {
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
}

FString UGameEngine::ResolveStartupScenePath() const
{
    if (StartupSettings.StartupScene.empty())
    {
        return {};
    }

    return FPaths::Normalize(FPaths::ToAbsoluteString(FPaths::ToWide(StartupSettings.StartupScene)));
}
