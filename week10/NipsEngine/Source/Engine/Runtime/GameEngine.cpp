#include "Engine/Runtime/GameEngine.h"

#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Engine/Input/GameplayInputTypes.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/World.h"
#include "Runtime/Script/ScriptManager.h"
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

    constexpr int RuntimeUILayoutWidth = 1920;
    constexpr int RuntimeUILayoutHeight = 1080;

    FInputFrame BuildStandaloneGameplayInputFrame(
        const InputSystem& Input,
        uint64 FrameNumber,
        HWND SourceWindow)
    {
        FInputFrame Frame{};
        Frame.FrameNumber = FrameNumber;
        Frame.SourceWindow = SourceWindow;
        Frame.MouseInputMode = Input.IsUsingRawMouse()
            ? EMouseInputMode::Relative
            : EMouseInputMode::Absolute;
        Frame.MouseScreenPos = Input.GetMousePos();
        Frame.MouseDelta = { Input.MouseDeltaX(), Input.MouseDeltaY() };
        Frame.WheelNotches = Input.GetScrollNotches();
        Frame.bLeftDragging = Input.GetLeftDragging();
        Frame.bMiddleDragging = Input.GetMiddleDragging();
        Frame.bRightDragging = Input.GetRightDragging();
        Frame.LeftDragVector = Input.GetLeftDragVector();
        Frame.MiddleDragVector = Input.GetMiddleDragVector();
        Frame.RightDragVector = Input.GetRightDragVector();

        for (int VK = 0; VK < 256; ++VK)
        {
            Frame.KeyDown[VK] = Input.GetKey(VK);
            Frame.KeyPressed[VK] = Input.GetKeyDown(VK);
            Frame.KeyReleased[VK] = Input.GetKeyUp(VK);
        }
        return Frame;
    }

    bool ParseBoolValue(const FString& Value, bool DefaultValue)
    {
        FString Lower = Value;
        std::transform(
            Lower.begin(),
            Lower.end(),
            Lower.begin(),
            [](unsigned char Ch)
            {
                return static_cast<char>(std::tolower(Ch));
            });
        if (Lower == "true" || Lower == "1" || Lower == "yes" || Lower == "on")
        {
            return true;
        }
        if (Lower == "false" || Lower == "0" || Lower == "no" || Lower == "off")
        {
            return false;
        }
        return DefaultValue;
    }

}

void UGameEngine::Init(FWindowsWindow* InWindow)
{
    const std::filesystem::path LogPath = std::filesystem::path(FPaths::RootDir()) / L"Saves" / L"Logs" / L"Game.log";
    FLog::SetFileOutputPath(LogPath.wstring());
    UE_LOG("[GameEngine] GameClient boot started.");

    UEngine::Init(InWindow);
    FScriptManager::Get().initializeLuaState();
    GetRmlUiSystem().Initialize(GetRenderer(), "GameClient", RuntimeUILayoutWidth, RuntimeUILayoutHeight);
    LoadGameSettings();
    LoadStartupWorld();
}

void UGameEngine::Shutdown()
{
    // Lua VM???대━湲??꾩뿉 Game World EndPlay瑜?癒쇱? ?몄텧?댁빞 ScriptComponent::EndPlay媛 ?덉쟾?섍쾶 ?ㅽ뻾?⑸땲??
    for (FWorldContext& Context : WorldList)
    {
        if (Context.World)
        {
            Context.World->EndPlay(EEndPlayReason::Type::Quit);
        }
    }

    // Lua Audio API濡??ъ깮???꾩뿭 ?ъ슫?쒕뒗 ?뱀젙 SoundComponent ?뚯쑀媛 ?꾨땲誘濡?蹂꾨룄濡??뺣━?⑸땲??
    GetAudioSystem().StopAll();

    for (FWorldContext& Context : WorldList)
    {
        if (Context.World)
        {
            UObjectManager::Get().DestroyObject(Context.World);
        }
    }
    WorldList.clear();
    ActiveWorldHandle = FName::None;
    GameMode = nullptr;
    PlayerController = nullptr;

    FScriptManager::Get().ShutdownLuaState();

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
    InputSystem& Input = InputSystem::Get();
    MaintainGameInputCapture(Input);
    Input.Tick();
    Input.SetGuiMouseCapture(false);
    Input.SetGuiKeyboardCapture(false);
    Input.SetGuiTextInputCapture(false);
    Input.SetGuiViewportMouseBlock(false);

    const bool bRuntimeUIConsumedInput = PumpRuntimeUIInput(Input);
    const FRuntimeInputPermissions InputPermissions =
        BuildRuntimeInputPermissions(Input.GetGuiInputState(), bRuntimeUIConsumedInput);
    if (InputPermissions.bAllowPlayerInput)
    {
        PumpPlayerInput(Input);
    }
    WorldTick(DeltaTime);
    ProcessPendingSceneOpen();
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

void UGameEngine::LoadGameSettings()
{
    const std::filesystem::path GameIniPath = std::filesystem::path(FPaths::SettingsDir()) / L"Game.ini";
    std::ifstream File(GameIniPath);
    if (!File.is_open())
    {
        UE_LOG_WARNING("[GameEngine] Settings/Game.ini not found. Using default startup scene: %s",
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
        else if (Key == "DefaultPawnClass")
        {
            StartupSettings.DefaultPawnClass = Value;
        }
        else if (Key == "DefaultPawnPrefabPath")
        {
            StartupSettings.DefaultPawnPrefabPath = FPaths::Normalize(Value);
        }
        else if (Key == "bShadow" || Key == "Shadow")
        {
            GetMutableRuntimeShowFlags().bShadow = ParseBoolValue(Value, GetRuntimeShowFlags().bShadow);
        }
        else if (Key == "bSkeletalMesh" || Key == "SkeletalMesh")
        {
            GetMutableRuntimeShowFlags().bSkeletalMesh = ParseBoolValue(Value, GetRuntimeShowFlags().bSkeletalMesh);
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
            CurrentScenePath = ScenePath;
            UE_LOG("[GameEngine] Loaded startup scene: %s", ScenePath.c_str());
            return;
        }

        UE_LOG_ERROR("[GameEngine] Failed to load startup scene: %s", ScenePath.c_str());
    }
    else
    {
        UE_LOG_ERROR("[GameEngine] Startup scene missing: %s", ScenePath.c_str());
    }

    FWorldContext& Context = CreateWorldContext(EWorldType::Game, FName("Game"), StartupSettings.GameName);
    SetActiveWorld(Context.ContextHandle);
    UE_LOG("[GameEngine] Created empty game world.");
}

void UGameEngine::OnSceneWorldWillUnload(UWorld* OldWorld)
{
    GameMode = nullptr;
    PlayerController = nullptr;
    GetRmlUiSystem().UnloadAllDocuments();
    GetAudioSystem().StopAll();
    if (OldWorld)
    {
        OldWorld->SetGlobalTimeScale(1.0f);
    }
}

void UGameEngine::OnSceneWorldLoaded(UWorld* NewWorld)
{
    GameMode = nullptr;
    PlayerController = nullptr;
    EnsurePlayerController();
    InputSystem::Get().SetUseRawMouse(false);
    MaintainGameInputCapture(InputSystem::Get());
}

AGameModeBase* UGameEngine::EnsureGameMode()
{
    if (GameMode)
    {
        return GameMode;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    GameMode = World->SpawnActor<AGameModeBase>();
    if (!GameMode)
    {
        UE_LOG_ERROR("[GameEngine] Failed to spawn GameMode.");
        return nullptr;
    }

    GameMode->SetFName(FName("GameMode"));
    FString PlayerControllerClass = StartupSettings.PlayerControllerClass.empty()
        ? FString("APlayerController")
        : StartupSettings.PlayerControllerClass;
    FString DefaultPawnClass = StartupSettings.DefaultPawnClass.empty()
        ? FString("ADefaultPawn")
        : StartupSettings.DefaultPawnClass;
    FString DefaultPawnPrefabPath = StartupSettings.DefaultPawnPrefabPath;

    const FWorldGameModeSettings& SceneGameModeSettings = World->GetGameModeSettings();
    if (SceneGameModeSettings.bOverrideGameMode)
    {
        if (!SceneGameModeSettings.PlayerControllerClass.empty())
        {
            PlayerControllerClass = SceneGameModeSettings.PlayerControllerClass;
        }
        if (!SceneGameModeSettings.DefaultPawnClass.empty())
        {
            DefaultPawnClass = SceneGameModeSettings.DefaultPawnClass;
        }
        DefaultPawnPrefabPath = SceneGameModeSettings.DefaultPawnPrefabPath;
    }

    GameMode->SetPlayerControllerClass(PlayerControllerClass);
    GameMode->SetDefaultPawnClass(DefaultPawnClass);
    GameMode->SetDefaultPawnPrefabPath(DefaultPawnPrefabPath);
    return GameMode;
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

    AGameModeBase* RuntimeGameMode = EnsureGameMode();
    if (!RuntimeGameMode)
    {
        return;
    }

    PlayerController = RuntimeGameMode->BootstrapPlayer(
        nullptr,
        Window ? static_cast<uint32>(Window->GetWidth()) : 1920u,
        Window ? static_cast<uint32>(Window->GetHeight()) : 1080u,
        nullptr);
    if (!PlayerController)
    {
        UE_LOG_ERROR("[GameEngine] Failed to spawn PlayerController class: %s", StartupSettings.PlayerControllerClass.c_str());
        return;
    }

    UE_LOG("[GameEngine] Spawned PlayerController class: %s", StartupSettings.PlayerControllerClass.c_str());
    if (FViewportCamera* RuntimeCamera = PlayerController->GetRuntimeCamera())
    {
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
    if (InputMode != ERuntimeInputMode::GameOnly || !IsRuntimeCursorLocked())
    {
        Input.SetUseRawMouse(false);
        Input.LockMouse(false);
        Input.SetCursorVisibility(IsRuntimeCursorVisible());
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
    const bool bAllowRuntimeUIInput =
        BuildRuntimeInputPermissions(Input.GetGuiInputState()).bAllowRuntimeUIInput;
    const bool bConsumed = GetRmlUiSystem().PumpGameInput(
        Input,
        Window,
        bAllowRuntimeUIInput,
        RuntimeUILayoutWidth,
        RuntimeUILayoutHeight);

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

    const FInputFrame Frame = BuildStandaloneGameplayInputFrame(
        Input,
        ++PlayerInputFrameCounter,
        Window ? Window->GetHWND() : nullptr);
    FGameplayInputSnapshot Snapshot = FDefaultGameplayInputMapping::BuildSnapshot(Frame);
    PlayerController->ProcessInputSnapshot(Snapshot);

    if (!bLoggedFirstInput)
    {
        for (int VK = 0; VK < 256; ++VK)
        {
            if (Input.GetKeyDown(VK))
            {
                UE_LOG("[GameEngine] First key input received: VK=%d", VK);
                bLoggedFirstInput = true;
                break;
            }
        }
    }

    if (!bLoggedFirstInput && Input.MouseMoved())
    {
        UE_LOG("[GameEngine] First mouse input received: dx=%d dy=%d", Input.MouseDeltaX(), Input.MouseDeltaY());
        bLoggedFirstInput = true;
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
