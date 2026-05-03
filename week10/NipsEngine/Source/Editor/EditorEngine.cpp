#include "Editor/EditorEngine.h"

#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Engine/Slate/SlateApplication.h"
#include "Engine/Input/InputSystem.h"
#include "Runtime/ViewportRect.h"
#include "Component/GizmoComponent.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/PrimitiveActors.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/World.h"
#include "Editor/EditorRenderPipeline.h"
#include "Core/Logging/Stats.h"
#include "Runtime/Script/ScriptManager.h"
#include "Slate/SSplitterV.h"
#include "Slate/SSplitterH.h"
#include "Settings/EditorSettings.h"
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
#include "RmlUi/Core/Input.h"
#include <algorithm>
#include <unordered_set>
#include <utility>

DEFINE_CLASS(UEditorEngine, UEngine)
REGISTER_FACTORY(UEditorEngine)

namespace
{
    bool HasPlayerStart(UWorld* World)
    {
        if (!World)
        {
            return false;
        }

        for (AActor* Actor : World->GetActors())
        {
            if (Actor && Actor->IsA<APlayerStart>())
            {
                return true;
            }
        }
        return false;
    }

    void SpawnDefaultSceneActors(UWorld* World)
    {
        if (!World)
        {
            return;
        }

        ADirectionalLightActor* DirectionalLight = World->SpawnActor<ADirectionalLightActor>();
        if (DirectionalLight)
        {
            DirectionalLight->InitDefaultComponents();
            DirectionalLight->SetFName(FName("Directional Light"));
            DirectionalLight->SetActorLocation(FVector(0.0f, 0.0f, 13.0f));
            DirectionalLight->SetActorRotation(FVector(0.0f, 44.0f, 0.0f));
        }

        AAmbientLightActor* AmbientLight = World->SpawnActor<AAmbientLightActor>();
        if (AmbientLight)
        {
            AmbientLight->InitDefaultComponents();
            AmbientLight->SetFName(FName("Ambient Light"));
            AmbientLight->SetActorLocation(FVector(0.0f, 0.0f, 15.0f));
        }

        if (!HasPlayerStart(World))
        {
            APlayerStart* PlayerStart = World->SpawnActor<APlayerStart>();
            if (PlayerStart)
            {
                PlayerStart->InitDefaultComponents();
                PlayerStart->SetFName(FName("Player Start"));
                PlayerStart->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
            }
        }

        World->SyncSpatialIndex();
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

//  Init
void UEditorEngine::Init(FWindowsWindow* InWindow)
{
    UEngine::Init(InWindow);
    InputSystem::Get().SetOwnerWindow(Window ? Window->GetHWND() : nullptr);
    EditorInputRouter.SetOwnerWindow(Window ? Window->GetHWND() : nullptr);
    FEditorSettings::Get().LoadFromFile(FEditorSettings::GetDefaultSettingsPath());

    MainPanel.Create(Window, Renderer, this);
    bool bCreatedStartupWorld = false;
    if (WorldList.empty())
    {
        CreateWorldContext(EWorldType::Editor, FName("Default"));
        bCreatedStartupWorld = true;
    }
    SetActiveWorld(WorldList[0].ContextHandle);
    ApplySpatialIndexMaintenanceSettings();
    if (bCreatedStartupWorld)
    {
        SpawnDefaultSceneActors(WorldList[0].World);
    }

    // Selection & Gizmo
    SelectionManager.Init();
    ViewportLayout.Init(InWindow, GetWorld(), &SelectionManager, this);
    GetFocusedWorld()->SetActiveCamera(GetCamera());

    // Slate 초기화 및 Viewport Layout 추가
    FSlateApplication::Get().Initialize();
    ViewportLayout.BuildViewportLayout(static_cast<int32>(Window->GetWidth()), static_cast<int32>(Window->GetHeight()));

    // Editor용 렌더 파이프라인 세팅
    SetRenderPipeline(std::make_unique<FEditorRenderPipeline>(this, Renderer));

	FScriptManager::Get().initializeLuaState();
}

void UEditorEngine::Shutdown()
{
    // 스플리터 비율을 Settings 에 기록 후 저장
    if (SSplitterV* SV = ViewportLayout.GetRootSplitterV())
        FEditorSettings::Get().SplitterVRatio = SV->GetSplitRatio();
    if (SSplitterH* SH = ViewportLayout.GetTopSplitterH())
        FEditorSettings::Get().SplitterHRatio = SH->GetSplitRatio();

    FEditorSettings::Get().SaveToFile(FEditorSettings::GetDefaultSettingsPath());

    CloseScene();
    SelectionManager.Shutdown();
    MainPanel.Release();
    
    // CloseScene 이후에 ViewportLayout을 내리면 Client 포인터 단절로 인한 역참조를 피할 수 있습니다.
    ViewportLayout.Shutdown();           // 위젯 트리 해제 (소유권: UEditorEngine)
    FSlateApplication::Get().Shutdown(); // RootWindow 해제
    ShutdownRmlUiRuntime();

    // 엔진 공통 해제 (Renderer, D3D 등)
    UEngine::Shutdown();
}

void UEditorEngine::OnWindowResized(uint32 Width, uint32 Height)
{
    UEngine::OnWindowResized(Width, Height);
    ViewportLayout.OnWindowResized(Width, Height);
}


void UEditorEngine::Tick(float DeltaTime)
{
    UpdateTimeState(DeltaTime);
    ProcessQueuedPlaySessionRequests();

    const FGuiInputState& GuiState = InputSystem::Get().GetGuiInputState();
    const bool bGuiKeyboardCaptureForViewport =
        (GuiState.bUsingKeyboard || GuiState.bUsingTextInput) && !GuiState.bAllowViewportMouseFocus;
    EditorInputRouter.SetImGuiCaptureState(GuiState.bUsingMouse, bGuiKeyboardCaptureForViewport);
    EditorInputRouter.SetForceViewportMouseBlock(GuiState.bBlockViewportMouse, GuiState.bAllowViewportMouseFocus);
    RegisterViewportInputTargets();
    if (PendingPIEViewportFocusFrames > 0 && GetEditorState() == EViewportPlayState::Playing)
    {
        const int32 FocusedIdx = (ActivePIEViewportIndex >= 0)
            ? ActivePIEViewportIndex
            : ViewportLayout.GetLastFocusedViewportIndex();
        if (FEditorViewportClient* FocusedClient = ViewportLayout.GetViewportClient(FocusedIdx))
        {
            ViewportLayout.SetLastFocusedViewportIndex(FocusedIdx);
            EditorInputRouter.SetImGuiCaptureState(false, false);
            EditorInputRouter.SetForceViewportMouseBlock(false, true);
            EditorInputRouter.ForceViewportFocus(FocusedClient->GetViewport());
        }
        InputSystem::Get().SetGuiMouseCapture(false);
        InputSystem::Get().SetGuiKeyboardCapture(false);
        InputSystem::Get().SetGuiTextInputCapture(false);
        InputSystem::Get().SetGuiViewportMouseBlock(false);
        InputSystem::Get().SetGuiViewportMouseFocusAllowed(true);
        --PendingPIEViewportFocusFrames;
    }
    FViewportInputContext RoutedInputContext;
    FInteractionBinding RoutedInputBinding;
    EditorInputRouter.Tick(DeltaTime, RoutedInputContext, RoutedInputBinding);
    for (int32 Index = 0; Index < FEditorViewportLayout::MaxViewports; ++Index)
    {
        if (FEditorViewportClient* ViewportClient = ViewportLayout.GetViewportClient(Index))
        {
            ViewportClient->SetLegacyInputSuppressedThisFrame(true);
        }
    }

    ViewportLayout.Tick(DeltaTime);
    MainPanel.Update();
    WorldTick(DeltaTime);
    Render(DeltaTime);
}

void UEditorEngine::RequestPIEViewportInputFocus(int32 FrameCount)
{
    PendingPIEViewportFocusFrames = std::max(PendingPIEViewportFocusFrames, FrameCount);
    MainPanel.RequestPIEViewportInputFocus();
}

class FEditorRmlUiActionEventListener final : public Rml::EventListener
{
public:
    explicit FEditorRmlUiActionEventListener(UEditorEngine* InOwner)
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
    UEditorEngine* Owner = nullptr;
};

void UEditorEngine::InitializeRmlUiRuntime()
{
    if (bRmlUiRuntimeInitialized)
    {
        return;
    }

    if (!RmlUiRenderInterface.Initialize(
        Renderer.GetFD3DDevice().GetDevice(),
        Renderer.GetFD3DDevice().GetDeviceContext()))
    {
        UE_LOG_ERROR("[RmlUi][PIE] Failed to initialize D3D11 render interface.");
        return;
    }

    if (!RmlUiRuntimeModule.Initialize())
    {
        UE_LOG_ERROR("[RmlUi][PIE] Failed to initialize runtime module.");
        RmlUiRenderInterface.Shutdown();
        return;
    }

    RmlUiContext = Rml::CreateContext("EditorPIE", Rml::Vector2i(1, 1), &RmlUiRenderInterface);
    if (!RmlUiContext)
    {
        UE_LOG_ERROR("[RmlUi][PIE] Failed to create context.");
        RmlUiRuntimeModule.Shutdown();
        RmlUiRenderInterface.Shutdown();
        return;
    }

    delete RmlUiActionListener;
    RmlUiActionListener = new FEditorRmlUiActionEventListener(this);
    bRmlUiRuntimeInitialized = true;
}

void UEditorEngine::ShutdownRmlUiRuntime()
{
    if (!bRmlUiRuntimeInitialized)
    {
        return;
    }

    if (RmlUiContext)
    {
        RmlUiContext->UnloadAllDocuments();
        RmlUiContext->Update();
        Rml::RemoveContext("EditorPIE");
        RmlUiContext = nullptr;
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

void UEditorEngine::RenderRuntimeUI(const FRuntimeUIRenderContext& Context)
{
    InitializeRmlUiRuntime();
    if (!bRmlUiRuntimeInitialized || !RmlUiContext)
    {
        return;
    }

    const int Width = std::max(static_cast<int>(Context.ViewportSize.X), 1);
    const int Height = std::max(static_cast<int>(Context.ViewportSize.Y), 1);
    RmlUiContext->SetDimensions(Rml::Vector2i(Width, Height));

    Renderer.UseBackBufferRenderTargets();
    RmlUiRenderInterface.BeginFrame(
        Rml::Vector2f(Context.ViewportMin.X, Context.ViewportMin.Y),
        Rml::Vector2f(Context.ViewportSize.X, Context.ViewportSize.Y));

    RmlUiContext->Update();
    RmlUiContext->Render();
}

bool UEditorEngine::LoadRmlUIDocument(const FString& ScreenId, const FString& Path)
{
    InitializeRmlUiRuntime();
    if (!bRmlUiRuntimeInitialized || !RmlUiContext || ScreenId.empty() || Path.empty())
    {
        return false;
    }

    UnloadRmlUIDocument(ScreenId);

    Rml::ElementDocument* Document = RmlUiContext->LoadDocument(Path);
    if (!Document)
    {
        UE_LOG_ERROR("[RmlUi][PIE] Failed to load document. Screen=%s Path=%s", ScreenId.c_str(), Path.c_str());
        return false;
    }

    AttachRmlUIDocumentListeners(Document);
    Document->Show();
    RmlUiDocumentsByScreenId[ScreenId] = Document;
    RmlUiDocumentPathByScreenId[ScreenId] = Path;
    return true;
}

bool UEditorEngine::UnloadRmlUIDocument(const FString& ScreenId)
{
    Rml::ElementDocument* Document = FindRmlUIDocument(ScreenId);
    if (!Document)
    {
        return false;
    }

    Document->Close();
    RmlUiDocumentsByScreenId.erase(ScreenId);
    return true;
}

bool UEditorEngine::ReloadRmlUIDocument(const FString& ScreenId)
{
    auto PathIt = RmlUiDocumentPathByScreenId.find(ScreenId);
    if (PathIt == RmlUiDocumentPathByScreenId.end())
    {
        return false;
    }

    const FString Path = PathIt->second;
    UnloadRmlUIDocument(ScreenId);
    return LoadRmlUIDocument(ScreenId, Path);
}

bool UEditorEngine::ShowRmlUIScreen(const FString& ScreenId)
{
    if (Rml::ElementDocument* Document = FindRmlUIDocument(ScreenId))
    {
        Document->Show();
        return true;
    }
    return false;
}

bool UEditorEngine::HideRmlUIScreen(const FString& ScreenId)
{
    if (Rml::ElementDocument* Document = FindRmlUIDocument(ScreenId))
    {
        Document->Hide();
        return true;
    }
    return false;
}

bool UEditorEngine::SetRmlUIElementText(const FString& ElementId, const FString& Text)
{
    if (Rml::Element* Element = FindRmlUIElement(ElementId))
    {
        Element->SetInnerRML(Text);
        return true;
    }
    return false;
}

bool UEditorEngine::SetRmlUIElementVisible(const FString& ElementId, bool bVisible)
{
    if (Rml::Element* Element = FindRmlUIElement(ElementId))
    {
        if (bVisible)
        {
            Element->RemoveProperty(Rml::PropertyId::Display);
        }
        else
        {
            Element->SetProperty(Rml::PropertyId::Display, Rml::Property(Rml::Style::Display::None));
        }
        return true;
    }
    return false;
}

bool UEditorEngine::SetRmlUIElementEnabled(const FString& ElementId, bool bEnabled)
{
    if (Rml::Element* Element = FindRmlUIElement(ElementId))
    {
        if (bEnabled)
        {
            Element->RemoveAttribute("disabled");
            Element->SetClass("disabled", false);
        }
        else
        {
            Element->SetAttribute("disabled", "disabled");
            Element->SetClass("disabled", true);
        }
        return true;
    }
    return false;
}

bool UEditorEngine::SetRmlUIElementClass(const FString& ElementId, const FString& ClassName, bool bEnabled)
{
    if (Rml::Element* Element = FindRmlUIElement(ElementId))
    {
        Element->SetClass(ClassName, bEnabled);
        return true;
    }
    return false;
}

bool UEditorEngine::SetRmlUIElementAttribute(const FString& ElementId, const FString& Name, const FString& Value)
{
    if (Rml::Element* Element = FindRmlUIElement(ElementId))
    {
        Element->SetAttribute(Name, Value);
        return true;
    }
    return false;
}

bool UEditorEngine::SetRmlUIElementStyle(const FString& ElementId, const FString& Name, const FString& Value)
{
    if (Rml::Element* Element = FindRmlUIElement(ElementId))
    {
        Element->SetProperty(Name, Value);
        return true;
    }
    return false;
}

TArray<FString> UEditorEngine::PollRmlUIActionEvents()
{
    TArray<FString> Events = RmlUiPendingActionEvents;
    RmlUiPendingActionEvents.clear();
    return Events;
}

void UEditorEngine::EnqueueRmlUIActionEvent(const FString& EventName)
{
    if (!EventName.empty())
    {
        RmlUiPendingActionEvents.push_back(EventName);
    }
}

Rml::ElementDocument* UEditorEngine::FindRmlUIDocument(const FString& ScreenId) const
{
    auto It = RmlUiDocumentsByScreenId.find(ScreenId);
    return It != RmlUiDocumentsByScreenId.end() ? It->second : nullptr;
}

Rml::Element* UEditorEngine::FindRmlUIElement(const FString& ElementId) const
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

void UEditorEngine::AttachRmlUIDocumentListeners(Rml::ElementDocument* Document)
{
    if (Document && RmlUiActionListener)
    {
        Document->AddEventListener("click", RmlUiActionListener);
    }
}

int UEditorEngine::GetRmlUiKeyModifierState(const InputSystem& Input) const
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

bool UEditorEngine::PumpPIERmlUiInput(const FViewportRect& ViewportRect)
{
    InputSystem& Input = InputSystem::Get();

    if (!bRmlUiRuntimeInitialized || !RmlUiContext || !Window || !Window->GetHWND()
        || ViewportRect.Width <= 0 || ViewportRect.Height <= 0)
    {
        return false;
    }

    if (GetRuntimeInputMode() == ERuntimeInputMode::GameOnly && IsRuntimeCursorLocked())
    {
        Input.ConsumeTextInput();
        return false;
    }

    POINT ClientMousePos = Input.GetMousePos();
    ::ScreenToClient(Window->GetHWND(), &ClientMousePos);

    const bool bInsideViewport =
        ClientMousePos.x >= ViewportRect.X && ClientMousePos.y >= ViewportRect.Y
        && ClientMousePos.x < ViewportRect.X + ViewportRect.Width
        && ClientMousePos.y < ViewportRect.Y + ViewportRect.Height;

    if (!bInsideViewport && !RmlUiContext->IsMouseInteracting())
    {
        return false;
    }

    RmlUiContext->SetDimensions(Rml::Vector2i(
        std::max(static_cast<int>(ViewportRect.Width), 1),
        std::max(static_cast<int>(ViewportRect.Height), 1)));

    const int Modifiers = GetRmlUiKeyModifierState(Input);
    bool bConsumed = false;

    if (bInsideViewport)
    {
        const int LocalX = ClientMousePos.x - ViewportRect.X;
        const int LocalY = ClientMousePos.y - ViewportRect.Y;
        const bool bMouseFree = RmlUiContext->ProcessMouseMove(LocalX, LocalY, Modifiers);
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

void UEditorEngine::RegisterViewportInputTargets()
{
    EditorInputRouter.ClearTargets();

    for (int32 Index = 0; Index < FEditorViewportLayout::MaxViewports; ++Index)
    {
        FSceneViewport& SceneViewport = ViewportLayout.GetSceneViewport(Index);
        FEditorViewportClient* ViewportClient = ViewportLayout.GetViewportClient(Index);
        if (!ViewportClient)
        {
            continue;
        }

        EditorInputRouter.RegisterTarget(
            &SceneViewport,
            ViewportClient,
            ViewportClient->GetPlayState() == EViewportPlayState::Playing ? EInteractionDomain::PIE : EInteractionDomain::Editor,
            [this, Index](FRect& OutRect)
            {
                const FViewportRect& ViewportRect = ViewportLayout.GetSceneViewport(Index).GetRect();
                if (ViewportRect.Width <= 0 || ViewportRect.Height <= 0)
                {
                    return false;
                }

                OutRect = FRect(
                    static_cast<float>(ViewportRect.X),
                    static_cast<float>(ViewportRect.Y),
                    static_cast<float>(ViewportRect.Width),
                    static_cast<float>(ViewportRect.Height));
                return true;
            },
            [this, Index]()
            {
                FEditorViewportClient* Client = ViewportLayout.GetViewportClient(Index);
                return Client ? Client->GetFocusedWorld() : nullptr;
            });
    }
}

void UEditorEngine::WorldTick(float DeltaTime)
{
    // 포커스된 뷰포트의 카메라를 해당 월드의 ActiveCamera로 설정합니다.
    // PIE Possessed 상태에서는 PlayerController의 RuntimeCamera가 게임 카메라를 소유하므로
    // 여기서 Editor viewport camera로 덮어쓰면 WASD/Mouse Look 결과가 화면에 반영되지 않습니다.
    const int32 FocusedIdx = ViewportLayout.GetLastFocusedViewportIndex();
    FEditorViewportClient* FocusedClient = ViewportLayout.GetViewportClient(FocusedIdx);
    if (FocusedClient && FocusedClient->AllowsEditorWorldControl())
    {
        if (UWorld* FocusedWorld = FocusedClient->GetFocusedWorld())
        {
            if (FViewportCamera* Cam = FocusedClient->GetCamera())
            {
                FocusedWorld->SetActiveCamera(Cam);
            }
        }
    }

    // WorldList의 모든 월드에 대해 Tick()을 넣어줍니다.
    // UWorld::Tick에서 EWorldType에 따라 TickEditor / TickGame이 분기됩니다.
    for (FWorldContext& Ctx : WorldList)
    {
        if (!Ctx.World || Ctx.bPaused)
            continue;
        Ctx.World->Tick(DeltaTime);
    }
}

int32 UEditorEngine::DeleteActors(const TArray<AActor*>& Actors)
{
    std::unordered_set<AActor*> SeenActors;
    TArray<std::pair<UWorld*, AActor*>> ActorsToDelete;

    for (AActor* Actor : Actors)
    {
        if (!Actor || SeenActors.find(Actor) != SeenActors.end())
        {
            continue;
        }

        UWorld* ActorWorld = Actor->GetFocusedWorld();
        if (!ActorWorld)
        {
            continue;
        }

        SeenActors.insert(Actor);
        ActorsToDelete.push_back(std::make_pair(ActorWorld, Actor));
    }

    if (ActorsToDelete.empty())
    {
        return 0;
    }

    CaptureUndoSnapshot("Delete Actors");

    int32 DeletedCount = 0;
    SelectionManager.BeginBatchUpdate();
    for (const std::pair<UWorld*, AActor*>& Entry : ActorsToDelete)
    {
        Entry.first->DestroyActor(Entry.second);
        ++DeletedCount;
    }
    SelectionManager.EndBatchUpdate();

    if (DeletedCount > 0)
    {
        MainPanel.GetSceneWidget().MarkSceneDirty();
        MainPanel.PushFooterLog(DeletedCount > 1 ? "Actors deleted" : "Actor deleted");
    }

    return DeletedCount;
}

FString UEditorEngine::CaptureSceneSnapshot() const
{
    UEditorEngine* MutableThis = const_cast<UEditorEngine*>(this);
    FWorldContext* Ctx = MutableThis->GetWorldContextFromHandle(ActiveWorldHandle);
    if (!Ctx || !Ctx->World)
    {
        return "";
    }

    return FSceneSaveManager::SaveToString(*Ctx, nullptr);
}

bool UEditorEngine::CaptureUndoSnapshot(const char* Reason)
{
    if (bRestoringUndoRedo || GetEditorState() != EViewportPlayState::Editing)
    {
        return false;
    }

    FString Snapshot = CaptureSceneSnapshot();
    if (Snapshot.empty())
    {
        return false;
    }

    if (!UndoHistory.empty() && UndoHistory.back().Snapshot == Snapshot)
    {
        return false;
    }

    FUndoSnapshotEntry Entry;
    Entry.Label = (Reason && Reason[0] != '\0') ? Reason : "Scene Edit";
    Entry.Snapshot = std::move(Snapshot);
    UndoHistory.push_back(std::move(Entry));
    if (static_cast<int32>(UndoHistory.size()) > MaxUndoHistory)
    {
        UndoHistory.erase(UndoHistory.begin());
    }
    if (!RedoHistory.empty())
    {
        RedoHistory.clear();
        MainPanel.PushFooterLog("Redo history cleared");
    }
    return true;
}

bool UEditorEngine::RestoreSceneSnapshot(const FString& Snapshot)
{
    if (Snapshot.empty())
    {
        return false;
    }

    FEditorCameraState CurrentCam;
    if (const FViewportCamera* Cam = GetCamera())
    {
        CurrentCam.Location = Cam->GetLocation();
        CurrentCam.Rotation = FRotator(Cam->GetRotation());
        CurrentCam.FOV = Cam->GetFOV() * (180.f / 3.14159265358979f);
        CurrentCam.NearClip = Cam->GetNearPlane();
        CurrentCam.FarClip = Cam->GetFarPlane();
        CurrentCam.bValid = true;
    }

    FWorldContext LoadCtx;
    FEditorCameraState LoadedCam;
    FSceneSaveManager::LoadFromString(Snapshot, LoadCtx, &LoadedCam);
    if (!LoadCtx.World)
    {
        return false;
    }

    bRestoringUndoRedo = true;
    MainPanel.ResetWidgetSelections();
    ClearScene();

    LoadCtx.WorldType = EWorldType::Editor;
    LoadCtx.ContextHandle = FName("UndoRedoScene");
    LoadCtx.ContextName = "Undo/Redo Scene";
    WorldList.push_back(LoadCtx);
    SetActiveWorld(LoadCtx.ContextHandle);
    ApplySpatialIndexMaintenanceSettings(LoadCtx.World);
    ResetViewport();

    const FEditorCameraState& CameraToRestore = LoadedCam.bValid ? LoadedCam : CurrentCam;
    if (CameraToRestore.bValid)
    {
        if (FViewportCamera* Cam = GetCamera())
        {
            Cam->SetLocation(CameraToRestore.Location);
            Cam->SetRotation(FQuat(CameraToRestore.Rotation));
            Cam->SetFOV(CameraToRestore.FOV * (3.14159265358979f / 180.f));
            Cam->SetNearPlane(CameraToRestore.NearClip);
            Cam->SetFarPlane(CameraToRestore.FarClip);
            if (FEditorViewportClient* Client = ViewportLayout.GetViewportClient(0))
            {
                Client->SyncCameraTarget();
            }
        }
    }

    if (UWorld* World = GetWorld())
    {
        World->RebuildSpatialIndex();
    }
    MainPanel.GetSceneWidget().MarkSceneDirty();
    bRestoringUndoRedo = false;
    return true;
}

bool UEditorEngine::Undo()
{
    if (bRestoringUndoRedo || UndoHistory.empty() || GetEditorState() != EViewportPlayState::Editing)
    {
        return false;
    }

    FString Current = CaptureSceneSnapshot();
    FUndoSnapshotEntry Previous = std::move(UndoHistory.back());
    UndoHistory.pop_back();
    if (!Current.empty())
    {
        RedoHistory.push_back(FUndoSnapshotEntry{ "Current", std::move(Current) });
        if (static_cast<int32>(RedoHistory.size()) > MaxUndoHistory)
        {
            RedoHistory.erase(RedoHistory.begin());
        }
    }

    const bool bRestored = RestoreSceneSnapshot(Previous.Snapshot);
    if (bRestored)
    {
        MainPanel.PushFooterLog("Undo: " + Previous.Label);
    }
    return bRestored;
}

bool UEditorEngine::Redo()
{
    if (bRestoringUndoRedo || RedoHistory.empty() || GetEditorState() != EViewportPlayState::Editing)
    {
        return false;
    }

    FString Current = CaptureSceneSnapshot();
    FUndoSnapshotEntry Next = std::move(RedoHistory.back());
    RedoHistory.pop_back();
    if (!Current.empty())
    {
        UndoHistory.push_back(FUndoSnapshotEntry{ "Current", std::move(Current) });
        if (static_cast<int32>(UndoHistory.size()) > MaxUndoHistory)
        {
            UndoHistory.erase(UndoHistory.begin());
        }
    }

    const bool bRestored = RestoreSceneSnapshot(Next.Snapshot);
    if (bRestored)
    {
        MainPanel.PushFooterLog("Redo: " + Next.Label);
    }
    return bRestored;
}

bool UEditorEngine::RestoreUndoHistoryIndex(int32 Index)
{
    if (bRestoringUndoRedo || GetEditorState() != EViewportPlayState::Editing)
    {
        return false;
    }
    if (Index < 0 || Index >= static_cast<int32>(UndoHistory.size()))
    {
        return false;
    }

    FUndoSnapshotEntry Target = UndoHistory[Index];
    FString Current = CaptureSceneSnapshot();

    RedoHistory.clear();
    if (!Current.empty())
    {
        RedoHistory.push_back(FUndoSnapshotEntry{ "Current", std::move(Current) });
    }
    for (int32 HistoryIndex = static_cast<int32>(UndoHistory.size()) - 1; HistoryIndex > Index; --HistoryIndex)
    {
        RedoHistory.push_back(std::move(UndoHistory[HistoryIndex]));
        if (static_cast<int32>(RedoHistory.size()) > MaxUndoHistory)
        {
            RedoHistory.erase(RedoHistory.begin());
        }
    }

    UndoHistory.erase(UndoHistory.begin() + Index, UndoHistory.end());

    const bool bRestored = RestoreSceneSnapshot(Target.Snapshot);
    if (bRestored)
    {
        MainPanel.PushFooterLog("History restored: " + Target.Label);
    }
    return bRestored;
}

void UEditorEngine::ClearUndoHistory()
{
    const bool bHadHistory = !UndoHistory.empty() || !RedoHistory.empty();
    UndoHistory.clear();
    RedoHistory.clear();
    if (bHadHistory)
    {
        MainPanel.PushFooterLog("Undo history cleared");
    }
}

FUndoHistoryStats UEditorEngine::GetUndoHistoryStats() const
{
    FUndoHistoryStats Stats;
    Stats.UndoCount = static_cast<int32>(UndoHistory.size());
    Stats.RedoCount = static_cast<int32>(RedoHistory.size());
    Stats.MaxEntries = MaxUndoHistory;

    auto Accumulate = [&Stats](const TArray<FUndoSnapshotEntry>& Entries)
    {
        for (const FUndoSnapshotEntry& Entry : Entries)
        {
            Stats.LogicalBytes += Entry.Label.size();
            Stats.LogicalBytes += Entry.Snapshot.size();
            Stats.ReservedBytes += Entry.Label.capacity();
            Stats.ReservedBytes += Entry.Snapshot.capacity();
        }
    };

    Accumulate(UndoHistory);
    Accumulate(RedoHistory);
    Stats.EntryOverheadBytes = (UndoHistory.size() + RedoHistory.size()) * sizeof(FUndoSnapshotEntry);
    Stats.ApproxTotalBytes = Stats.ReservedBytes + Stats.EntryOverheadBytes;
    return Stats;
}

void UEditorEngine::RenderUI(float DeltaTime)
{
    MainPanel.Render(DeltaTime);
}

void UEditorEngine::StartPlaySession()
{
    const EViewportPlayState CurrentState = GetEditorState();
    if (CurrentState == EViewportPlayState::Paused)
    {
        ResumePlaySession();
        return;
    }

    if (CurrentState == EViewportPlayState::Playing)
    {
        return;
    }

    bStartPlaySessionQueued = true;
    bStopPlaySessionQueued = false;
}

void UEditorEngine::ProcessQueuedPlaySessionRequests()
{
    if (bStopPlaySessionQueued)
    {
        bStopPlaySessionQueued = false;
        bStartPlaySessionQueued = false;
        StopPlaySessionNow();
        return;
    }

    if (bStartPlaySessionQueued)
    {
        bStartPlaySessionQueued = false;
        StartPlaySessionNow();
    }
}

void UEditorEngine::StartPlaySessionNow()
{
    const EViewportPlayState CurrentState = GetEditorState();

    if (CurrentState == EViewportPlayState::Paused)
    {
        ResumePlaySession();
        return;
    }
    if (CurrentState == EViewportPlayState::Playing) return;

	// 포커스된 뷰포트 클라이언트를 찾고 카메라 상태를 저장한 뒤, 실행 상태를 변경합니다.
    const int32 FocusedIdx = ViewportLayout.GetLastFocusedViewportIndex();
    FEditorViewportClient* FocusedClient = ViewportLayout.GetViewportClient(FocusedIdx);
    UWorld* FocusedWorld = GetFocusedWorld();

    if (!FocusedClient || !FocusedWorld) return;
    if (!HasPlayerStart(FocusedWorld))
    {
        UE_LOG_ERROR("[PIE] Cannot start Play In Editor: Player Start is missing.");
        MainPanel.PushFooterLog("PIE failed: Player Start is missing");
        return;
    }

    FocusedClient->SaveCameraSnapshot();
    ActivePIEViewportIndex = FocusedIdx;
	// 주의! Editor State는 실제 에디터의 상태가 아닌, 현재 에디터가 포커스한 뷰포트의 상태를 의미합니다.
    SetEditorState(EViewportPlayState::Playing); 

    // PIE 월드 복제하고 세팅한 뒤, RegisterWorld() 헬퍼를 사용해 월드를 WorldList에 등록합니다.
    UWorld* PIEWorld = Cast<UWorld>(FocusedWorld->Duplicate());
    PIEWorld->SetWorldType(EWorldType::PIE);
    FName PIEHandle(("PIE_" + std::to_string(FocusedIdx)).c_str());
    std::string PIEName = "PIE_World_" + std::to_string(FocusedIdx);
    
    RegisterWorld(PIEWorld, EWorldType::PIE, PIEHandle, PIEName);
    ViewportPIEHandles[FocusedIdx] = PIEHandle;

    // 월드를 전환한 뒤 뷰포트에 연결하고, PIE World를 실행합니다.
    SetActiveWorld(PIEHandle);
    FocusedClient->StartPIE(PIEWorld);
    FocusedClient->SetEndPIECallback([this]() { StopPlaySession(); });
    MainPanel.HideEditorWindowsForPIE();

    FocusedClient->LockCursorToViewport();
    InputSystem::Get().SetCursorVisibility(false);
    InputSystem::Get().SetGuiMouseCapture(false);
    InputSystem::Get().SetGuiKeyboardCapture(false);
    InputSystem::Get().SetGuiTextInputCapture(false);
    InputSystem::Get().SetGuiViewportMouseBlock(false);
    InputSystem::Get().SetGuiViewportMouseFocusAllowed(true);
    EditorInputRouter.SetImGuiCaptureState(false, false);
    EditorInputRouter.SetForceViewportMouseBlock(false, true);
    EditorInputRouter.ForceViewportFocus(FocusedClient->GetViewport());
    RequestPIEViewportInputFocus(3);
    SelectionManager.ClearSelection();

    constexpr const char* DefaultPIEPlayerControllerClass = "APlayerController";
    APlayerController* PlayerController = Cast<APlayerController>(
        PIEWorld->SpawnActorByTypeName(DefaultPIEPlayerControllerClass));
    if (PlayerController)
    {
        PlayerController->InitDefaultComponents();
        PlayerController->SetFName(FName(DefaultPIEPlayerControllerClass));
        PlayerController->ConfigureRuntimeCameraFromViewport(FocusedClient->GetCamera());
        FocusedClient->SetPIEPlayerController(PlayerController);
        PIEWorld->SetActiveCamera(PlayerController->GetRuntimeCamera());
    }
    else
    {
        PIEWorld->SetActiveCamera(FocusedClient->GetCamera());
    }
    PIEWorld->BeginPlay();
}

void UEditorEngine::PausePlaySession()
{
    if (GetEditorState() != EViewportPlayState::Playing)
        return;

    SetEditorState(EViewportPlayState::Paused);

    // PIE 컨텍스트를 일시정지 상태로 표시해 WorldTick에서 제외합니다.
    const int32 FocusedIdx = (ActivePIEViewportIndex >= 0) ? ActivePIEViewportIndex : ViewportLayout.GetLastFocusedViewportIndex();
    auto HandleIt = ViewportPIEHandles.find(FocusedIdx);
    if (HandleIt != ViewportPIEHandles.end())
        if (FWorldContext* Ctx = GetWorldContextFromHandle(HandleIt->second))
            Ctx->bPaused = true;
}

void UEditorEngine::ResumePlaySession()
{
    const int32 ResumeIdx = (ActivePIEViewportIndex >= 0) ? ActivePIEViewportIndex : ViewportLayout.GetLastFocusedViewportIndex();
    auto ResumeIt = ViewportPIEHandles.find(ResumeIdx);

    if (ResumeIt != ViewportPIEHandles.end())
    {
        if (FWorldContext* Ctx = GetWorldContextFromHandle(ResumeIt->second))
        {
            Ctx->bPaused = false;
        }
    }

    SetEditorState(EViewportPlayState::Playing);
}

void UEditorEngine::StopPlaySession()
{
    if (GetEditorState() == EViewportPlayState::Editing && ViewportPIEHandles.empty())
    {
        bStopPlaySessionQueued = false;
        return;
    }

    bStopPlaySessionQueued = true;
    bStartPlaySessionQueued = false;
}

void UEditorEngine::StopPlaySessionNow()
{
    if (GetEditorState() == EViewportPlayState::Editing && ViewportPIEHandles.empty())
        return;

    int32 FocusedIdx = (ActivePIEViewportIndex >= 0) ? ActivePIEViewportIndex : ViewportLayout.GetLastFocusedViewportIndex();
    if (ViewportPIEHandles.find(FocusedIdx) == ViewportPIEHandles.end() && !ViewportPIEHandles.empty())
    {
        FocusedIdx = ViewportPIEHandles.begin()->first;
    }
    FEditorViewportClient* FocusedClient = ViewportLayout.GetViewportClient(FocusedIdx);

    // 기존 PIE 월드를 해제합니다.
    auto HandleIt = ViewportPIEHandles.find(FocusedIdx);
    if (HandleIt != ViewportPIEHandles.end())
    {
        FName PIEHandle = HandleIt->second;
        ViewportPIEHandles.erase(HandleIt);
        
        UnregisterWorld(PIEHandle);

        // PIE에서 Lua Audio API로 직접 재생한 전역 사운드는 Actor EndPlay 소유가 아니므로
        // 마지막 PIE 세션이 끝날 때 명시적으로 정리합니다.
        if (ViewportPIEHandles.empty())
        {
            GetAudioSystem().StopAll();
        }
    }

    // 원본 에디터 월드를 검색합니다.
    FName EditorHandle = GetEditorWorldHandle();
    UWorld* EditorWorld = nullptr;
    
    if (EditorHandle != FName::None)
    {
        SetActiveWorld(EditorHandle);
        if (FWorldContext* Ctx = GetWorldContextFromHandle(EditorHandle))
        {
            EditorWorld = Ctx->World;
        }
    }

    // 원본 에디터 월드로 뷰포트 및 상태를 복구합니다.
    ViewportLayout.SetLastFocusedViewportIndex(FocusedIdx);
    FocusedClient->EndPIE(EditorWorld);
    SetEditorState(EViewportPlayState::Editing);
    FocusedClient->RestoreCameraSnapshot();
    ActivePIEViewportIndex = -1;
    MainPanel.RestoreEditorWindowsAfterPIE();

    if (ViewportPIEHandles.empty())
    {
        InputSystem::Get().SetCursorVisibility(true);
    }

    SelectionManager.ClearSelection();
}

void UEditorEngine::ResetViewport()
{
    for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
    {
        FEditorViewportClient* ViewportClient = ViewportLayout.GetViewportClient(i);
        if (!ViewportClient)
        {
            continue;
        }
        ViewportClient->CreateCamera();
        ViewportClient->SetWorld(GetWorld());
        ViewportClient->ApplyCameraMode();
    }

    // 디폴트로 0번 뷰포트의 카메라를 월드 활성 카메라로 재등록
    if (UWorld* ActiveWorld = GetWorld())
    {
        ActiveWorld->SetActiveCamera(ViewportLayout.GetIndexedViewportClientCamera(0));
    }
}

void UEditorEngine::SetActiveWorld(const FName& Handle)
{
    UWorld* PreviousWorld = GetWorld();
    if (PreviousWorld)
    {
        UnbindActorDestroyedListener(PreviousWorld);
    }

    UEngine::SetActiveWorld(Handle);

    if (UWorld* ActiveWorld = GetWorld())
    {
        BindActorDestroyedListener(ActiveWorld);
    }
}

void UEditorEngine::CloseScene()
{
    SelectionManager.ClearSelection();
    UnbindActorDestroyedListener(ActorDestroyedListenerWorld);

    for (FWorldContext& Ctx : WorldList)
    {
        Ctx.World->EndPlay(EEndPlayReason::Type::EndPlayInEditor);
        UObjectManager::Get().DestroyObject(Ctx.World);
    }
    WorldList.clear();
    ActiveWorldHandle = FName::None;

    for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
    {
        FEditorViewportClient* ViewportClient = ViewportLayout.GetViewportClient(i);
        if (!ViewportClient)
        {
            continue;
        }
        ViewportClient->DestroyCamera();
        ViewportClient->SetWorld(nullptr);
    }
}

void UEditorEngine::NewScene()
{
    ClearScene();
    FWorldContext& Ctx = CreateWorldContext(EWorldType::Editor, FName("NewScene"), "New Scene");
    SetActiveWorld(Ctx.ContextHandle);
    ApplySpatialIndexMaintenanceSettings(Ctx.World);
    SpawnDefaultSceneActors(Ctx.World);

    ResetViewport();
}

void UEditorEngine::ApplySpatialIndexMaintenanceSettings(UWorld* TargetWorld)
{
    // Init 초반에는 ViewportLayout이 아직 연결되지 않았을 수 있으므로
    // FocusedWorld보다 ActiveWorld(GetWorld) 경로를 우선 사용한다.
    UWorld* World = (TargetWorld != nullptr) ? TargetWorld : GetWorld();
    if (World == nullptr)
    {
        World = GetFocusedWorld();
        if (World == nullptr)
        {
            return;
        }
    }

    const FEditorSettings& Settings = GetSettings();
    FWorldSpatialIndex::FMaintenancePolicy& Policy = World->GetSpatialIndex().GetMaintenancePolicy();

    Policy.BatchRefitMinDirtyCount = std::max<int32>(1, Settings.SpatialBatchRefitMinDirtyCount);
    Policy.BatchRefitDirtyPercentThreshold = std::clamp<int32>(Settings.SpatialBatchRefitDirtyPercentThreshold, 1, 100);
    Policy.RotationStructuralChangeThreshold = std::max<int32>(1, Settings.SpatialRotationStructuralChangeThreshold);
    Policy.RotationDirtyCountThreshold = std::max<int32>(1, Settings.SpatialRotationDirtyCountThreshold);
    Policy.RotationDirtyPercentThreshold = std::clamp<int32>(Settings.SpatialRotationDirtyPercentThreshold, 1, 100);
}

FViewportCamera* UEditorEngine::GetCamera()
{
    return ViewportLayout.GetIndexedViewportClientCamera(0);
}

const FViewportCamera* UEditorEngine::GetCamera() const
{
    return ViewportLayout.GetIndexedViewportClientCamera(0);
}

FEditorRenderPipeline* UEditorEngine::GetEditorRenderPipeline() const
{
    return static_cast<FEditorRenderPipeline*>(GetRenderPipeline());
}

void UEditorEngine::ClearScene()
{
    SelectionManager.ClearSelection();
    UnbindActorDestroyedListener(ActorDestroyedListenerWorld);

    for (FWorldContext& Ctx : WorldList)
    {
        Ctx.World->EndPlay(EEndPlayReason::Type::LevelTransition);
        UObjectManager::Get().DestroyObject(Ctx.World);
    }

    WorldList.clear();
    ActiveWorldHandle = FName::None;

    for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
    {
        FEditorViewportClient* ViewportClient = ViewportLayout.GetViewportClient(i);
        if (!ViewportClient)
        {
            continue;
        }
        ViewportClient->DestroyCamera();
        ViewportClient->SetWorld(nullptr);
    }
}

// 이미 생성된 월드를 컨텍스트에 등록합니다.
FWorldContext& UEditorEngine::RegisterWorld(UWorld* InWorld, EWorldType Type, const FName& Handle, const std::string& Name)
{
    FWorldContext Context;
    Context.WorldType = Type;
    Context.World = InWorld;
    Context.ContextName = Name;
    Context.ContextHandle = Handle;
    
    WorldList.push_back(Context);
    return WorldList.back();
}

// 컨텍스트에서 월드를 찾아 파괴하고 리스트에서 제거합니다.
void UEditorEngine::UnregisterWorld(const FName& Handle)
{
    for (auto it = WorldList.begin(); it != WorldList.end(); ++it)
    {
        if (it->ContextHandle == Handle)
        {
            if (it->World)
            {
                if (it->World == ActorDestroyedListenerWorld)
                {
                    UnbindActorDestroyedListener(it->World);
                }
                it->World->EndPlay(EEndPlayReason::Type::EndPlayInEditor);
                UObjectManager::Get().DestroyObject(it->World);
            }
            WorldList.erase(it);
            return; // 찾아서 지웠으므로 즉시 종료
        }
    }
}

// Editor Context World 핸들을 찾아 반환합니다.
FName UEditorEngine::GetEditorWorldHandle() const
{
    for (const FWorldContext& Ctx : WorldList)
    {
        if (Ctx.WorldType == EWorldType::Editor)
        {
            return Ctx.ContextHandle;
        }
    }
    return FName::None;
}

void UEditorEngine::HandleActorDestroyed(AActor* Actor)
{
    SelectionManager.OnActorDestroyed(Actor);
    MainPanel.GetPropertyWidget().OnActorDestroyed(Actor);
    MainPanel.GetMaterialWidget().OnActorDestroyed(Actor);
}

void UEditorEngine::BindActorDestroyedListener(UWorld* World)
{
    if (!World)
    {
        return;
    }

    if (ActorDestroyedListenerWorld == World && ActorDestroyedListenerId != 0)
    {
        return;
    }

    if (ActorDestroyedListenerWorld && ActorDestroyedListenerWorld != World)
    {
        UnbindActorDestroyedListener(ActorDestroyedListenerWorld);
    }

    ActorDestroyedListenerId = World->AddActorDestroyedListener(
        [this](AActor* Actor)
        {
            HandleActorDestroyed(Actor);
        });
    ActorDestroyedListenerWorld = World;
}

void UEditorEngine::UnbindActorDestroyedListener(UWorld* World)
{
    UWorld* TargetWorld = ActorDestroyedListenerWorld ? ActorDestroyedListenerWorld : World;
    if (!TargetWorld || ActorDestroyedListenerId == 0)
    {
        return;
    }

    TargetWorld->RemoveActorDestroyedListener(ActorDestroyedListenerId);
    ActorDestroyedListenerId = 0;
    ActorDestroyedListenerWorld = nullptr;
}
