#include "Engine/Runtime/GameEngine.h"

#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/World.h"
#include "Runtime/Script/ScriptManager.h"
#include "Serialization/SceneSaveManager.h"

#ifdef GetFirstChild
#undef GetFirstChild
#endif
#ifdef GetNextSibling
#undef GetNextSibling
#endif
#include "RmlUi/Core.h"
#include "RmlUi/Core/Context.h"
#include "RmlUi/Core/Element.h"
#include "RmlUi/Core/ElementDocument.h"
#include "RmlUi/Core/Event.h"
#include "RmlUi/Core/EventListener.h"
#include "RmlUi/Core/Factory.h"
#include "RmlUi/Core/Input.h"
#include "RmlUi/Core/Types.h"

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

    Rml::Input::KeyIdentifier MapVirtualKeyToRmlKey(int VK)
    {
        using namespace Rml::Input;

        if (VK >= '0' && VK <= '9')
        {
            return static_cast<KeyIdentifier>(KI_0 + (VK - '0'));
        }
        if (VK >= 'A' && VK <= 'Z')
        {
            return static_cast<KeyIdentifier>(KI_A + (VK - 'A'));
        }
        if (VK >= VK_F1 && VK <= VK_F24)
        {
            return static_cast<KeyIdentifier>(KI_F1 + (VK - VK_F1));
        }
        if (VK >= VK_NUMPAD0 && VK <= VK_NUMPAD9)
        {
            return static_cast<KeyIdentifier>(KI_NUMPAD0 + (VK - VK_NUMPAD0));
        }

        switch (VK)
        {
        case VK_SPACE: return KI_SPACE;
        case VK_BACK: return KI_BACK;
        case VK_TAB: return KI_TAB;
        case VK_RETURN: return KI_RETURN;
        case VK_ESCAPE: return KI_ESCAPE;
        case VK_PRIOR: return KI_PRIOR;
        case VK_NEXT: return KI_NEXT;
        case VK_END: return KI_END;
        case VK_HOME: return KI_HOME;
        case VK_LEFT: return KI_LEFT;
        case VK_UP: return KI_UP;
        case VK_RIGHT: return KI_RIGHT;
        case VK_DOWN: return KI_DOWN;
        case VK_INSERT: return KI_INSERT;
        case VK_DELETE: return KI_DELETE;
        case VK_SHIFT: return KI_LSHIFT;
        case VK_LSHIFT: return KI_LSHIFT;
        case VK_RSHIFT: return KI_RSHIFT;
        case VK_CONTROL: return KI_LCONTROL;
        case VK_LCONTROL: return KI_LCONTROL;
        case VK_RCONTROL: return KI_RCONTROL;
        case VK_MENU: return KI_LMENU;
        case VK_LMENU: return KI_LMENU;
        case VK_RMENU: return KI_RMENU;
        case VK_OEM_1: return KI_OEM_1;
        case VK_OEM_PLUS: return KI_OEM_PLUS;
        case VK_OEM_COMMA: return KI_OEM_COMMA;
        case VK_OEM_MINUS: return KI_OEM_MINUS;
        case VK_OEM_PERIOD: return KI_OEM_PERIOD;
        case VK_OEM_2: return KI_OEM_2;
        case VK_OEM_3: return KI_OEM_3;
        case VK_OEM_4: return KI_OEM_4;
        case VK_OEM_5: return KI_OEM_5;
        case VK_OEM_6: return KI_OEM_6;
        case VK_OEM_7: return KI_OEM_7;
        case VK_MULTIPLY: return KI_MULTIPLY;
        case VK_ADD: return KI_ADD;
        case VK_SEPARATOR: return KI_SEPARATOR;
        case VK_SUBTRACT: return KI_SUBTRACT;
        case VK_DECIMAL: return KI_DECIMAL;
        case VK_DIVIDE: return KI_DIVIDE;
        case VK_PAUSE: return KI_PAUSE;
        case VK_CAPITAL: return KI_CAPITAL;
        case VK_NUMLOCK: return KI_NUMLOCK;
        case VK_SCROLL: return KI_SCROLL;
        default: return KI_UNKNOWN;
        }
    }
}

void UGameEngine::Init(FWindowsWindow* InWindow)
{
    const std::filesystem::path LogPath = std::filesystem::path(FPaths::RootDir()) / L"Saves" / L"Logs" / L"Game.log";
    FLog::SetFileOutputPath(LogPath.wstring());
    UE_LOG("[GameEngine] GameClient boot started.");

    UEngine::Init(InWindow);
    FScriptManager::Get().initializeLuaState();
    InitializeRmlUiRuntime();
    LoadGameSettings();
    LoadStartupWorld();
}

void UGameEngine::Shutdown()
{
    // Lua VM을 내리기 전에 Game World EndPlay를 먼저 호출해야 ScriptComponent::EndPlay가 안전하게 실행됩니다.
    for (FWorldContext& Context : WorldList)
    {
        if (Context.World)
        {
            Context.World->EndPlay(EEndPlayReason::Type::Quit);
        }
    }

    // Lua Audio API로 재생한 전역 사운드는 특정 SoundComponent 소유가 아니므로 별도로 정리합니다.
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
    PlayerController = nullptr;

    FScriptManager::Get().ShutdownLuaState();
    ShutdownRmlUiRuntime();

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

void UGameEngine::RenderRuntimeUI(const FRuntimeUIRenderContext& Context)
{
    RenderRmlUiTestDocument(Context);
}

bool UGameEngine::LoadRmlUIDocument(const FString& ScreenId, const FString& Path)
{
    if (!bRmlUiRuntimeInitialized || !RmlUiContext || ScreenId.empty() || Path.empty())
    {
        return false;
    }

    UnloadRmlUIDocument(ScreenId);
    Rml::Factory::ClearStyleSheetCache();
    Rml::Factory::ClearTemplateCache();

    Rml::ElementDocument* Document = RmlUiContext->LoadDocument(Path);
    if (!Document)
    {
        UE_LOG_ERROR("[RmlUi] Failed to load document. Screen=%s Path=%s", ScreenId.c_str(), Path.c_str());
        return false;
    }

    AttachRmlUIDocumentListeners(Document);
    Document->Show();
    RmlUiDocumentsByScreenId[ScreenId] = Document;
    RmlUiDocumentPathByScreenId[ScreenId] = Path;
    UE_LOG("[RmlUi] Loaded document. Screen=%s Path=%s", ScreenId.c_str(), Path.c_str());
    return true;
}

bool UGameEngine::UnloadRmlUIDocument(const FString& ScreenId)
{
    Rml::ElementDocument* Document = FindRmlUIDocument(ScreenId);
    if (!Document || !RmlUiContext)
    {
        return false;
    }

    RmlUiContext->UnloadDocument(Document);
    RmlUiContext->Update();
    RmlUiDocumentsByScreenId.erase(ScreenId);
    RmlUiDocumentPathByScreenId.erase(ScreenId);
    return true;
}

bool UGameEngine::ReloadRmlUIDocument(const FString& ScreenId)
{
    auto It = RmlUiDocumentPathByScreenId.find(ScreenId);
    if (It == RmlUiDocumentPathByScreenId.end())
    {
        return false;
    }

    const FString Path = It->second;
    UnloadRmlUIDocument(ScreenId);
    return LoadRmlUIDocument(ScreenId, Path);
}

bool UGameEngine::ShowRmlUIScreen(const FString& ScreenId)
{
    Rml::ElementDocument* Document = FindRmlUIDocument(ScreenId);
    if (!Document)
    {
        return false;
    }

    Document->Show();
    return true;
}

bool UGameEngine::HideRmlUIScreen(const FString& ScreenId)
{
    Rml::ElementDocument* Document = FindRmlUIDocument(ScreenId);
    if (!Document)
    {
        return false;
    }

    Document->Hide();
    return true;
}

bool UGameEngine::HasRmlUIElement(const FString& ElementId)
{
    return FindRmlUIElement(ElementId) != nullptr;
}

FString UGameEngine::GetRmlUIElementText(const FString& ElementId)
{
    Rml::Element* Element = FindRmlUIElement(ElementId);
    return Element ? Element->GetInnerRML() : "";
}

bool UGameEngine::SetRmlUIElementText(const FString& ElementId, const FString& Text)
{
    Rml::Element* Element = FindRmlUIElement(ElementId);
    if (!Element)
    {
        return false;
    }

    Element->SetInnerRML(Text);
    return true;
}

bool UGameEngine::SetRmlUIElementVisible(const FString& ElementId, bool bVisible)
{
    Rml::Element* Element = FindRmlUIElement(ElementId);
    if (!Element)
    {
        return false;
    }

    if (bVisible)
    {
        Element->RemoveProperty("display");
    }
    else
    {
        Element->SetProperty("display", "none");
    }
    return true;
}

bool UGameEngine::SetRmlUIElementEnabled(const FString& ElementId, bool bEnabled)
{
    Rml::Element* Element = FindRmlUIElement(ElementId);
    if (!Element)
    {
        return false;
    }

    if (bEnabled)
    {
        Element->RemoveAttribute("disabled");
    }
    else
    {
        Element->SetAttribute("disabled", "disabled");
    }
    Element->SetClass("disabled", !bEnabled);
    return true;
}

bool UGameEngine::SetRmlUIElementClass(const FString& ElementId, const FString& ClassName, bool bEnabled)
{
    Rml::Element* Element = FindRmlUIElement(ElementId);
    if (!Element)
    {
        return false;
    }

    Element->SetClass(ClassName, bEnabled);
    return true;
}

bool UGameEngine::HasRmlUIElementClass(const FString& ElementId, const FString& ClassName)
{
    Rml::Element* Element = FindRmlUIElement(ElementId);
    return Element ? Element->IsClassSet(ClassName) : false;
}

FString UGameEngine::GetRmlUIElementClassNames(const FString& ElementId)
{
    Rml::Element* Element = FindRmlUIElement(ElementId);
    return Element ? Element->GetClassNames() : "";
}

bool UGameEngine::SetRmlUIElementClassNames(const FString& ElementId, const FString& ClassNames)
{
    Rml::Element* Element = FindRmlUIElement(ElementId);
    if (!Element)
    {
        return false;
    }

    Element->SetClassNames(ClassNames);
    return true;
}

bool UGameEngine::HasRmlUIElementAttribute(const FString& ElementId, const FString& Name)
{
    Rml::Element* Element = FindRmlUIElement(ElementId);
    return Element ? Element->HasAttribute(Name) : false;
}

FString UGameEngine::GetRmlUIElementAttribute(const FString& ElementId, const FString& Name)
{
    Rml::Element* Element = FindRmlUIElement(ElementId);
    return Element ? Element->GetAttribute<Rml::String>(Name, "") : "";
}

bool UGameEngine::SetRmlUIElementAttribute(const FString& ElementId, const FString& Name, const FString& Value)
{
    Rml::Element* Element = FindRmlUIElement(ElementId);
    if (!Element)
    {
        return false;
    }

    Element->SetAttribute(Name, Value);
    return true;
}

bool UGameEngine::RemoveRmlUIElementAttribute(const FString& ElementId, const FString& Name)
{
    Rml::Element* Element = FindRmlUIElement(ElementId);
    if (!Element)
    {
        return false;
    }

    Element->RemoveAttribute(Name);
    return true;
}

FString UGameEngine::GetRmlUIElementStyle(const FString& ElementId, const FString& Name)
{
    Rml::Element* Element = FindRmlUIElement(ElementId);
    if (!Element)
    {
        return "";
    }

    const Rml::Property* Property = Element->GetProperty(Name);
    return Property ? Property->ToString() : "";
}

bool UGameEngine::SetRmlUIElementStyle(const FString& ElementId, const FString& Name, const FString& Value)
{
    Rml::Element* Element = FindRmlUIElement(ElementId);
    if (!Element)
    {
        return false;
    }

    return Element->SetProperty(Name, Value);
}

bool UGameEngine::RemoveRmlUIElementStyle(const FString& ElementId, const FString& Name)
{
    Rml::Element* Element = FindRmlUIElement(ElementId);
    if (!Element)
    {
        return false;
    }

    Element->RemoveProperty(Name);
    return true;
}

bool UGameEngine::FocusRmlUIElement(const FString& ElementId, bool bFocusVisible)
{
    Rml::Element* Element = FindRmlUIElement(ElementId);
    return Element ? Element->Focus(bFocusVisible) : false;
}

bool UGameEngine::BlurRmlUIElement(const FString& ElementId)
{
    Rml::Element* Element = FindRmlUIElement(ElementId);
    if (!Element)
    {
        return false;
    }

    Element->Blur();
    return true;
}

bool UGameEngine::ClickRmlUIElement(const FString& ElementId)
{
    Rml::Element* Element = FindRmlUIElement(ElementId);
    if (!Element)
    {
        return false;
    }

    Element->Click();
    return true;
}

TArray<FString> UGameEngine::PollRmlUIActionEvents()
{
    TArray<FString> Events = RmlUiPendingActionEvents;
    RmlUiPendingActionEvents.clear();
    return Events;
}

void UGameEngine::EnqueueRmlUIActionEvent(const FString& EventName)
{
    if (!EventName.empty())
    {
        RmlUiPendingActionEvents.push_back(EventName);
    }
}

class FRmlUiActionEventListener final : public Rml::EventListener
{
public:
    explicit FRmlUiActionEventListener(UGameEngine* InOwner)
        : Owner(InOwner)
    {
    }

    void ProcessEvent(Rml::Event& Event) override
    {
        if (!Owner)
        {
            return;
        }

        Rml::Element* Element = Event.GetTargetElement();
        while (Element)
        {
            Rml::String Action = Element->GetAttribute<Rml::String>("data-action", "");
            if (Action.empty())
            {
                Action = Element->GetAttribute<Rml::String>("action", "");
            }

            if (!Action.empty())
            {
                Owner->EnqueueRmlUIActionEvent(Action);
                return;
            }

            Element = Element->GetParentNode();
        }
    }

private:
    UGameEngine* Owner = nullptr;
};

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
    }
}

void UGameEngine::InitializeRmlUiRuntime()
{
    if (bRmlUiRuntimeInitialized)
    {
        return;
    }

    if (!RmlUiRenderInterface.Initialize(
        Renderer.GetFD3DDevice().GetDevice(),
        Renderer.GetFD3DDevice().GetDeviceContext()))
    {
        UE_LOG_ERROR("[RmlUi] Failed to initialize runtime render interface.");
        return;
    }

    if (!RmlUiRuntimeModule.Initialize())
    {
        RmlUiRenderInterface.Shutdown();
        return;
    }

    const int Width = Window ? std::max(static_cast<int>(Window->GetWidth()), 1) : 1280;
    const int Height = Window ? std::max(static_cast<int>(Window->GetHeight()), 1) : 720;
    RmlUiContext = Rml::CreateContext("GameClient", Rml::Vector2i(Width, Height), &RmlUiRenderInterface);
    if (!RmlUiContext)
    {
        UE_LOG_ERROR("[RmlUi] Failed to create GameClient context.");
        RmlUiRuntimeModule.Shutdown();
        RmlUiRenderInterface.Shutdown();
        return;
    }

    delete RmlUiActionListener;
    RmlUiActionListener = new FRmlUiActionEventListener(this);
    bRmlUiRuntimeInitialized = true;

    if (LoadRmlUIDocument("RuntimeSmokeTest", "Asset/UI/Test/RuntimeSmokeTest.rml"))
    {
        RmlUiTestDocument = FindRmlUIDocument("RuntimeSmokeTest");
    }
    else
    {
        UE_LOG_ERROR("[RmlUi] Failed to load runtime smoke test document.");
    }
}

void UGameEngine::ShutdownRmlUiRuntime()
{
    if (!bRmlUiRuntimeInitialized)
    {
        return;
    }

    if (RmlUiContext)
    {
        RmlUiContext->UnloadAllDocuments();
        RmlUiContext->Update();
        Rml::RemoveContext("GameClient");
        RmlUiContext = nullptr;
        RmlUiTestDocument = nullptr;
    }

    delete RmlUiActionListener;
    RmlUiActionListener = nullptr;
    RmlUiPendingActionEvents.clear();
    RmlUiDocumentsByScreenId.clear();
    RmlUiDocumentPathByScreenId.clear();
    RmlUiRuntimeModule.Shutdown();
    RmlUiRenderInterface.Shutdown();
    bRmlUiRuntimeInitialized = false;
}

void UGameEngine::UnloadAllRmlUIDocuments()
{
    if (!RmlUiContext)
    {
        return;
    }

    TArray<FString> ScreenIds;
    for (const auto& Pair : RmlUiDocumentsByScreenId)
    {
        ScreenIds.push_back(Pair.first);
    }

    for (const FString& ScreenId : ScreenIds)
    {
        UnloadRmlUIDocument(ScreenId);
    }

    RmlUiDocumentPathByScreenId.clear();
    RmlUiPendingActionEvents.clear();
    RmlUiTestDocument = nullptr;
}

void UGameEngine::RenderRmlUiTestDocument(const FRuntimeUIRenderContext& Context)
{
    if (!bRmlUiRuntimeInitialized || !RmlUiContext)
    {
        return;
    }

    const int LayoutWidth = std::max(static_cast<int>(Context.LayoutSize.X > 0.0f ? Context.LayoutSize.X : Context.ViewportSize.X), 1);
    const int LayoutHeight = std::max(static_cast<int>(Context.LayoutSize.Y > 0.0f ? Context.LayoutSize.Y : Context.ViewportSize.Y), 1);
    RmlUiContext->SetDimensions(Rml::Vector2i(LayoutWidth, LayoutHeight));

    Renderer.UseBackBufferRenderTargets();
    RmlUiRenderInterface.BeginFrame(
        Rml::Vector2f(Context.ViewportMin.X, Context.ViewportMin.Y),
        Rml::Vector2f(Context.ViewportSize.X, Context.ViewportSize.Y),
        Rml::Vector2f(
            Context.ViewportSize.X / static_cast<float>(LayoutWidth),
            Context.ViewportSize.Y / static_cast<float>(LayoutHeight)));

    RmlUiContext->Update();
    RmlUiContext->Render();
}

Rml::ElementDocument* UGameEngine::FindRmlUIDocument(const FString& ScreenId) const
{
    auto It = RmlUiDocumentsByScreenId.find(ScreenId);
    return It != RmlUiDocumentsByScreenId.end() ? It->second : nullptr;
}

Rml::Element* UGameEngine::FindRmlUIElement(const FString& ElementId) const
{
    if (ElementId.empty())
    {
        return nullptr;
    }

    for (const auto& Pair : RmlUiDocumentsByScreenId)
    {
        if (Pair.second)
        {
            if (Rml::Element* Element = Pair.second->GetElementById(ElementId))
            {
                return Element;
            }
        }
    }

    return nullptr;
}

void UGameEngine::AttachRmlUIDocumentListeners(Rml::ElementDocument* Document)
{
    if (!Document || !RmlUiActionListener)
    {
        return;
    }

    Document->AddEventListener("click", RmlUiActionListener);
    Document->AddEventListener("change", RmlUiActionListener);
    Document->AddEventListener("submit", RmlUiActionListener);
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
    PlayerController = nullptr;
    UnloadAllRmlUIDocuments();
    GetAudioSystem().StopAll();
    SetTimeScale(1.0f);
}

void UGameEngine::OnSceneWorldLoaded(UWorld* NewWorld)
{
    PlayerController = nullptr;
    EnsurePlayerController();
    InputSystem::Get().SetUseRawMouse(false);
    MaintainGameInputCapture(InputSystem::Get());
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
        UE_LOG_ERROR("[GameEngine] Failed to spawn PlayerController class: %s", StartupSettings.PlayerControllerClass.c_str());
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
    const bool bConsumed = PumpRmlUiInput(Input);

    if (bConsumed)
    {
        Input.SetGuiMouseCapture(true);
        Input.SetGuiViewportMouseBlock(true);
    }

    return bConsumed;
}

int UGameEngine::GetRmlUiKeyModifierState(const InputSystem& Input) const
{
    int Modifiers = 0;
    if (Input.GetKey(VK_CONTROL) || Input.GetKey(VK_LCONTROL) || Input.GetKey(VK_RCONTROL))
    {
        Modifiers |= Rml::Input::KM_CTRL;
    }
    if (Input.GetKey(VK_SHIFT) || Input.GetKey(VK_LSHIFT) || Input.GetKey(VK_RSHIFT))
    {
        Modifiers |= Rml::Input::KM_SHIFT;
    }
    if (Input.GetKey(VK_MENU) || Input.GetKey(VK_LMENU) || Input.GetKey(VK_RMENU))
    {
        Modifiers |= Rml::Input::KM_ALT;
    }
    if ((::GetKeyState(VK_CAPITAL) & 0x0001) != 0)
    {
        Modifiers |= Rml::Input::KM_CAPSLOCK;
    }
    if ((::GetKeyState(VK_NUMLOCK) & 0x0001) != 0)
    {
        Modifiers |= Rml::Input::KM_NUMLOCK;
    }
    if ((::GetKeyState(VK_SCROLL) & 0x0001) != 0)
    {
        Modifiers |= Rml::Input::KM_SCROLLLOCK;
    }
    return Modifiers;
}

bool UGameEngine::PumpRmlUiInput(InputSystem& Input)
{
    if (!bRmlUiRuntimeInitialized || !RmlUiContext || !Window || !Window->GetHWND())
    {
        return false;
    }

    // GameOnly에서 커서가 숨겨진 상태는 플레이어 컨트롤 전용이다.
    // RmlUi HUD는 렌더만 하고, 입력은 UI 모드에서만 받도록 분리한다.
    if (GetRuntimeInputMode() == ERuntimeInputMode::GameOnly && IsRuntimeCursorLocked())
    {
        Input.ConsumeTextInput();
        return false;
    }

    const int Width = std::max(static_cast<int>(Window->GetWidth()), 1);
    const int Height = std::max(static_cast<int>(Window->GetHeight()), 1);
    RmlUiContext->SetDimensions(Rml::Vector2i(Width, Height));

    POINT ClientMousePos = Input.GetMousePos();
    ::ScreenToClient(Window->GetHWND(), &ClientMousePos);

    const int Modifiers = GetRmlUiKeyModifierState(Input);
    bool bConsumed = false;

    const bool bInsideClient =
        ClientMousePos.x >= 0 && ClientMousePos.y >= 0
        && ClientMousePos.x < Width && ClientMousePos.y < Height;

    if (bInsideClient)
    {
        const bool bMouseFree = RmlUiContext->ProcessMouseMove(ClientMousePos.x, ClientMousePos.y, Modifiers);
        bConsumed = (!bMouseFree && RmlUiContext->IsMouseInteracting()) || bConsumed;
    }
    else
    {
        const bool bMouseFree = RmlUiContext->ProcessMouseLeave();
        bConsumed = (!bMouseFree) || bConsumed;
    }

    auto PumpMouseButton = [&](int VK, int ButtonIndex)
    {
        if (Input.GetKeyDown(VK))
        {
            const bool bMouseFree = RmlUiContext->ProcessMouseButtonDown(ButtonIndex, Modifiers);
            bConsumed = (!bMouseFree) || bConsumed;
        }
        if (Input.GetKeyUp(VK))
        {
            const bool bMouseFree = RmlUiContext->ProcessMouseButtonUp(ButtonIndex, Modifiers);
            bConsumed = (!bMouseFree) || bConsumed;
        }
    };

    PumpMouseButton(VK_LBUTTON, 0);
    PumpMouseButton(VK_RBUTTON, 1);
    PumpMouseButton(VK_MBUTTON, 2);

    if (Input.GetScrollDelta() != 0)
    {
        const float WheelDelta = -Input.GetScrollNotches();
        const bool bEventNotConsumed = RmlUiContext->ProcessMouseWheel(Rml::Vector2f(0.0f, WheelDelta), Modifiers);
        bConsumed = (!bEventNotConsumed) || bConsumed;
    }

    bool bKeyboardConsumed = false;
    for (int VK = 0; VK < 256; ++VK)
    {
        if (IsMouseButtonVK(VK))
        {
            continue;
        }

        const Rml::Input::KeyIdentifier Key = MapVirtualKeyToRmlKey(VK);
        if (Key == Rml::Input::KI_UNKNOWN)
        {
            continue;
        }

        if (Input.GetKeyDown(VK))
        {
            const bool bEventNotConsumed = RmlUiContext->ProcessKeyDown(Key, Modifiers);
            bKeyboardConsumed = (!bEventNotConsumed) || bKeyboardConsumed;
        }
        if (Input.GetKeyUp(VK))
        {
            const bool bEventNotConsumed = RmlUiContext->ProcessKeyUp(Key, Modifiers);
            bKeyboardConsumed = (!bEventNotConsumed) || bKeyboardConsumed;
        }
    }

    for (uint32_t Codepoint : Input.ConsumeTextInput())
    {
        const bool bEventNotConsumed = RmlUiContext->ProcessTextInput(static_cast<Rml::Character>(Codepoint));
        bKeyboardConsumed = (!bEventNotConsumed) || bKeyboardConsumed;
    }

    bConsumed = bKeyboardConsumed || bConsumed;

    if (bConsumed)
    {
        Input.SetGuiMouseCapture(true);
        Input.SetGuiViewportMouseBlock(true);
        if (bKeyboardConsumed)
        {
            Input.SetGuiKeyboardCapture(true);
        }
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
