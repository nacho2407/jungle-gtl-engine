#include "Editor.h"

#include "Viewport/EditorViewportClient.h"

#include "Engine/Game/Actor.h"
#include "Panel/ControlPanel.h"
#include "Panel/OutlinerPanel.h"
#include "Panel/PanelManager.h"
#include "Panel/PropertiesPanel.h"
#include "Panel/StatePanel.h"

#include "imgui.h"
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

namespace
{
#ifdef IMGUI_HAS_DOCK
    constexpr ImGuiDockNodeFlags RootDockSpaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
#endif

    constexpr char AboutPopupId[] = "About##EditorAbout";
    constexpr float RestoredSideGutter = 3.0f;
    constexpr float RestoredBottomGutter = 3.0f;
    constexpr ImU32 GutterColor = IM_COL32(46, 46, 48, 255);

    struct FEditorGutterMetrics
    {
        float Left = 0.0f;
        float Right = 0.0f;
        float Bottom = 0.0f;
    };

    FEditorGutterMetrics GetDockSpaceGutterMetrics(const IEditorChromeHost* Host)
    {
        const bool bIsMaximized = Host != nullptr && Host->IsWindowMaximized();
        if (bIsMaximized)
        {
            return {};
        }

        FEditorGutterMetrics Metrics;
        Metrics.Left = RestoredSideGutter;
        Metrics.Right = RestoredSideGutter;
        Metrics.Bottom = RestoredBottomGutter;
        return Metrics;
    }

    void DrawDockSpaceGutters(ImGuiViewport* Viewport, const FEditorGutterMetrics& Metrics)
    {
        if (Viewport == nullptr)
        {
            return;
        }

        const float Top = FEditorChrome::TitleBarHeight;
        const float ContentTop = Viewport->Pos.y + Top;
        const float ContentBottom = Viewport->Pos.y + Viewport->Size.y;
        if (ContentBottom <= ContentTop)
        {
            return;
        }

        ImDrawList* DrawList = ImGui::GetBackgroundDrawList(Viewport);
        if (DrawList == nullptr)
        {
            return;
        }

        if (Metrics.Left > 0.0f)
        {
            DrawList->AddRectFilled(ImVec2(Viewport->Pos.x, ContentTop),
                                    ImVec2(Viewport->Pos.x + Metrics.Left, ContentBottom),
                                    GutterColor);
        }

        if (Metrics.Right > 0.0f)
        {
            DrawList->AddRectFilled(
                ImVec2(Viewport->Pos.x + Viewport->Size.x - Metrics.Right, ContentTop),
                ImVec2(Viewport->Pos.x + Viewport->Size.x, ContentBottom), GutterColor);
        }

        if (Metrics.Bottom > 0.0f)
        {
            DrawList->AddRectFilled(
                ImVec2(Viewport->Pos.x, Viewport->Pos.y + Viewport->Size.y - Metrics.Bottom),
                ImVec2(Viewport->Pos.x + Viewport->Size.x, Viewport->Pos.y + Viewport->Size.y),
                GutterColor);
        }
    }

    FString BuildPanelCommandId(const FPanelDescriptor& Descriptor)
    {
        return "panel.toggle." + std::to_string(Descriptor.PanelType.hash_code());
    }
} // namespace

//class FSamplePanel : public IPanel
//{
//public:
//    explicit FSamplePanel(const FEditorLogBuffer* InLogBuffer)
//        : LogBuffer(InLogBuffer)
//    {
//    }
//
//    const wchar_t* GetPanelID() const override { return L"SamplePanel"; }
//    const wchar_t* GetDisplayName() const override { return L"Sample Panel"; }
//    bool ShouldOpenByDefault() const override { return true; }
//
//    void Draw() override
//    {
//        if (ImGui::Begin("Sample Panel", nullptr))
//        {
//            ImGui::Text("PanelManager registration test panel");
//            ImGui::Separator();
//
//
//            if (LogBuffer != nullptr)
//            {
//                for (const auto& Log : LogBuffer->GetLogBuffer())
//                {
//                    ImGui::Spacing();
//                    ImGui::Text("%s", Log.Message.c_str());
//                }
//            }
//        }
//        ImGui::End();
//    }
//
//private:
//    const FEditorLogBuffer* LogBuffer = nullptr;
//};


void FEditor::Create()
{
    //  LOG
    GLog = &LogBuffer;
    
    //  TODO : Viewport Client
    EditorContext.Editor = this;

    ViewportClient.Create();
    ViewportClient.SetEditorContext(&EditorContext);

    // 메뉴 시스템은 command 등록과 배치 등록을 분리해서 초기화합니다.
    MenuRegistry.Clear();
    RegisterDefaultCommands();
    RegisterDefaultMenus();

    PanelManager = new FPanelManager();
    PanelManager->Initialize(&EditorContext);
    // 새 패널이 등록되면 Window 메뉴 항목도 함께 자동 등록합니다.
    PanelManager->SetPanelDescriptorRegisteredCallback(
        [this](const FPanelDescriptor& Descriptor)
        {
            RegisterWindowPanelCommand(Descriptor);
        });
    PanelManager->RegisterPanelInstance<FControlPanel>();
    PanelManager->RegisterPanelInstance<FOutlinerPanel>();
    PanelManager->RegisterPanelInstance<FPropertiesPanel>();
    PanelManager->RegisterPanelInstance<FStatePanel>();

    //PanelManager->RegisterPanelInstance<FSamplePanel>(&LogBuffer);

    //  TODO : Gizmo
    

    //  TEMP SCENE
    CurScene = new FScene();
    ViewportClient.SetScene(CurScene);
    
    UE_LOG(FEditor, ELogVerbosity::Log, "Hello Editor");
    EditorContext.Scene = CurScene;
}

void FEditor::Release()
{
    ViewportClient.Release();

    if (PanelManager != nullptr)
    {
        PanelManager->Shutdown();
        delete PanelManager;
        PanelManager = nullptr;
    }

    delete CurScene;
    CurScene = nullptr;
    EditorContext.Scene = nullptr;

    MenuRegistry.Clear();
    ChromeHost = nullptr;
    EditorChrome.SetHost(nullptr);

    if (GLog == &LogBuffer)
    {
        GLog = nullptr;
    }
}

void FEditor::Initialize()
{
    if (CurScene == nullptr)
    {
        CurScene = new FScene();
        ViewportClient.SetScene(CurScene);
        EditorContext.Scene = CurScene;
    }
}

void FEditor::SetChromeHost(IEditorChromeHost* InChromeHost)
{
    ChromeHost = InChromeHost;
    EditorChrome.SetHost(InChromeHost);
}

void FEditor::Tick(float DeltaTime, Engine::ApplicationCore::FInputSystem* InputSystem)
{
    EditorContext.DeltaTime = DeltaTime;
    Engine::ApplicationCore::FInputEvent Event;

    while (InputSystem->PollEvent(Event))
    {
        ViewportClient.HandleInputEvent(Event, InputSystem->GetInputState());
    }

    ViewportClient.Tick(DeltaTime, InputSystem->GetInputState());

    if (PanelManager != nullptr)
    {
        PanelManager->Tick(DeltaTime);
    }

    BuildRenderData();
}

void FEditor::OnWindowResized(float Width, float Height)
{
    if (Width <= 0 || Height <= 0)
    {
        return;
    }

    WindowHeight = Height;
    WindowWidth = Width;
    EditorContext.WindowWidth = Width;
    EditorContext.WindowHeight = Height;
    ViewportClient.OnResize(static_cast<uint32>(Width), static_cast<uint32>(Height));
}

void FEditor::CreateNewScene()
{
    ClearScene();
}

void FEditor::ClearScene()
{
}

void FEditor::SetSelectedObject(UObject* InSelectedObject)
{
    EditorContext.SelectedActors.clear();
    if (AActor* SelectedActor = Cast<AActor>(InSelectedObject))
    {
        EditorContext.SelectedActors.push_back(SelectedActor);
    }

    EditorContext.SelectedObject = InSelectedObject;
    ViewportClient.SyncSelectionFromContext();
}

void FEditor::RequestAboutPopup()
{
    bRequestOpenAboutPopup = true;
    bAboutPopupOpen = true;
}

void FEditor::RegisterDefaultCommands()
{
    // 여기서는 "무엇을 실행할지"만 등록하고, 메뉴 어디에 둘지는 RegisterDefaultMenus에서 정합니다.
    MenuRegistry.RegisterCommand(FEditorCommandDefinition{
        .CommandId = "file.new_scene",
        .Label = L"새 씬",
        .ShortcutLabel = "Ctrl+N",
        .Execute =
            [this]()
            {
                CreateNewScene();
            }});

    MenuRegistry.RegisterCommand(FEditorCommandDefinition{
        .CommandId = "file.clear_scene",
        .Label = L"씬 비우기",
        .Execute =
            [this]()
            {
                ClearScene();
            }});

    MenuRegistry.RegisterCommand(FEditorCommandDefinition{
        .CommandId = "file.exit",
        .Label = L"종료",
        .ShortcutLabel = "Alt+F4",
        .Execute =
            [this]()
            {
                if (ChromeHost != nullptr)
                {
                    ChromeHost->CloseWindow();
                }
            }});

    MenuRegistry.RegisterCommand(FEditorCommandDefinition{
        .CommandId = "edit.undo",
        .Label = L"실행 취소",
        .ShortcutLabel = "Ctrl+Z",
        .CanExecute =
            []()
            {
                return false;
            }});

    MenuRegistry.RegisterCommand(FEditorCommandDefinition{
        .CommandId = "edit.redo",
        .Label = L"다시 실행",
        .ShortcutLabel = "Ctrl+Y",
        .CanExecute =
            []()
            {
                return false;
            }});

    MenuRegistry.RegisterCommand(FEditorCommandDefinition{
        .CommandId = "edit.preferences",
        .Label = L"설정 준비 중",
        .CanExecute =
            []()
            {
                return false;
            }});

    MenuRegistry.RegisterCommand(FEditorCommandDefinition{
        .CommandId = "tool.content_browser",
        .Label = L"콘텐츠 브라우저 준비 중",
        .CanExecute =
            []()
            {
                return false;
            }});

    MenuRegistry.RegisterCommand(FEditorCommandDefinition{
        .CommandId = "tool.build",
        .Label = L"빌드 준비 중",
        .CanExecute =
            []()
            {
                return false;
            }});

    MenuRegistry.RegisterCommand(FEditorCommandDefinition{
        .CommandId = "help.about",
        .Label = L"정보",
        .ShortcutLabel = "F1",
        .Execute =
            [this]()
            {
                RequestAboutPopup();
            }});
}

void FEditor::RegisterDefaultMenus()
{
    // 여기서는 이미 등록된 command를 어떤 메뉴/순서에 배치할지만 정의합니다.
    MenuRegistry.RegisterMenuItem(
        FEditorMenuEntryDefinition{.MainMenu = EEditorMainMenu::File,
                                   .CommandId = "file.new_scene",
                                   .Order = 0});
    MenuRegistry.RegisterMenuItem(
        FEditorMenuEntryDefinition{.MainMenu = EEditorMainMenu::File,
                                   .CommandId = "file.clear_scene",
                                   .Order = 10});
    MenuRegistry.RegisterMenuSeparator(EEditorMainMenu::File, {}, 20);
    MenuRegistry.RegisterMenuItem(
        FEditorMenuEntryDefinition{.MainMenu = EEditorMainMenu::File,
                                   .CommandId = "file.exit",
                                   .Order = 30});

    MenuRegistry.RegisterMenuItem(
        FEditorMenuEntryDefinition{.MainMenu = EEditorMainMenu::Edit,
                                   .CommandId = "edit.undo",
                                   .Order = 0});
    MenuRegistry.RegisterMenuItem(
        FEditorMenuEntryDefinition{.MainMenu = EEditorMainMenu::Edit,
                                   .CommandId = "edit.redo",
                                   .Order = 10});
    MenuRegistry.RegisterMenuSeparator(EEditorMainMenu::Edit, {}, 20);
    MenuRegistry.RegisterMenuItem(
        FEditorMenuEntryDefinition{.MainMenu = EEditorMainMenu::Edit,
                                   .CommandId = "edit.preferences",
                                   .Order = 30});

    MenuRegistry.RegisterMenuItem(
        FEditorMenuEntryDefinition{.MainMenu = EEditorMainMenu::Tool,
                                   .CommandId = "tool.content_browser",
                                   .Order = 0});
    MenuRegistry.RegisterMenuItem(
        FEditorMenuEntryDefinition{.MainMenu = EEditorMainMenu::Tool,
                                   .CommandId = "tool.build",
                                   .Order = 10});

    MenuRegistry.RegisterMenuItem(
        FEditorMenuEntryDefinition{.MainMenu = EEditorMainMenu::Help,
                                   .CommandId = "help.about",
                                   .Order = 0});
}

void FEditor::RegisterWindowPanelCommand(const FPanelDescriptor& Descriptor)
{
    if (!Descriptor.bShowInWindowMenu)
    {
        return;
    }

    // PanelDescriptor를 Window 메뉴용 체크형 command로 변환합니다.
    const FString CommandId = BuildPanelCommandId(Descriptor);
    const std::type_index PanelType = Descriptor.PanelType;

    MenuRegistry.RegisterCommand(FEditorCommandDefinition{
        .CommandId = CommandId,
        .Label = Descriptor.DisplayName,
        .Execute =
            [this, PanelType]()
            {
                if (PanelManager == nullptr)
                {
                    return;
                }

                FPanelOpenRequest Request;
                Request.PanelType = PanelType;
                PanelManager->TogglePanel(Request);
            },
        .CanExecute =
            [this]()
            {
                return PanelManager != nullptr;
            },
        .IsChecked =
            [this, PanelType]()
            {
                if (PanelManager == nullptr)
                {
                    return false;
                }

                FPanelOpenRequest Request;
                Request.PanelType = PanelType;
                if (IPanel* Panel = PanelManager->FindPanel(Request))
                {
                    return Panel->IsOpen();
                }

                return false;
            },
        .bCheckable = true});

    MenuRegistry.RegisterMenuItem(FEditorMenuEntryDefinition{
        .MainMenu = EEditorMainMenu::Window,
        .SubmenuPath = Descriptor.WindowMenuPath,
        .CommandId = CommandId,
        .Order = Descriptor.WindowMenuOrder});
}

void FEditor::DrawAboutPopup()
{
    if (bRequestOpenAboutPopup)
    {
        ImGui::OpenPopup(AboutPopupId);
        bRequestOpenAboutPopup = false;
    }

    if (ImGui::BeginPopupModal(AboutPopupId, &bAboutPopupOpen,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Jungle Editor");
        ImGui::Separator();
        ImGui::TextUnformatted("Custom title bar menu prototype");
        ImGui::TextUnformatted("Registry driven File/Edit/Window/Tool/Help menu");
        ImGui::Spacing();
        if (ImGui::Button("Close"))
        {
            bAboutPopupOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void FEditor::BuildSceneView()
{
    SceneView.SetViewMatrix(ViewportClient.GetCamera().GetViewMatrix());
    SceneView.SetProjectionMatrix(ViewportClient.GetCamera().GetProjectionMatrix());
    SceneView.SetViewLocation(ViewportClient.GetCamera().GetLocation());

    FViewportRect ViewRect;
    ViewRect.X = 0;
    ViewRect.Y = 0;
    ViewRect.Width = static_cast<int32>(WindowWidth);
    ViewRect.Height = static_cast<int32>(WindowHeight);

    SceneView.SetViewRect(ViewRect);
    SceneView.SetClipPlanes(ViewportClient.GetCamera().GetNearPlane(),
                            ViewportClient.GetCamera().GetFarPlane());
}

void FEditor::DrawRootDockSpace()
{
#ifdef IMGUI_HAS_DOCK
    if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) == 0)
    {
        return;
    }

    ImGuiViewport* Viewport = ImGui::GetMainViewport();
    if (Viewport == nullptr)
    {
        return;
    }

    const FEditorGutterMetrics GutterMetrics = GetDockSpaceGutterMetrics(ChromeHost);
    DrawDockSpaceGutters(Viewport, GutterMetrics);

    const float DockSpaceWidth =
        (Viewport->Size.x > (GutterMetrics.Left + GutterMetrics.Right))
            ? (Viewport->Size.x - GutterMetrics.Left - GutterMetrics.Right)
            : 0.0f;
    const float DockSpaceHeight =
        (Viewport->Size.y > (FEditorChrome::TitleBarHeight + GutterMetrics.Bottom))
            ? (Viewport->Size.y - FEditorChrome::TitleBarHeight - GutterMetrics.Bottom)
            : 0.0f;

    if (DockSpaceWidth <= 0.0f || DockSpaceHeight <= 0.0f)
    {
        return;
    }

    ImGui::SetNextWindowPos(
        ImVec2(Viewport->Pos.x + GutterMetrics.Left,
               Viewport->Pos.y + FEditorChrome::TitleBarHeight));
    ImGui::SetNextWindowSize(ImVec2(DockSpaceWidth, DockSpaceHeight));
    ImGui::SetNextWindowViewport(Viewport->ID);

    ImGuiWindowFlags WindowFlags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoSavedSettings;

    if ((RootDockSpaceFlags & ImGuiDockNodeFlags_PassthruCentralNode) != 0)
    {
        WindowFlags |= ImGuiWindowFlags_NoBackground;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (ImGui::Begin("##EditorRootDockSpace", nullptr, WindowFlags))
    {
        ImGui::DockSpace(ImGui::GetID("EditorRootDockSpace"), ImVec2(0.0f, 0.0f),
                         RootDockSpaceFlags);
    }

    ImGui::End();
    ImGui::PopStyleVar(3);
#endif
}

void FEditor::DrawPanel()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();

    ImGui::NewFrame();
    // 매 프레임 현재 enabled/checked 상태를 평가해서 chrome이 소비할 메뉴 스냅샷을 만듭니다.
    const TArray<FEditorChromeMenu> ChromeMenus = MenuRegistry.BuildChromeMenus();
    DrawRootDockSpace();

    if (PanelManager != nullptr)
    {
        PanelManager->DrawPanels();
    }

    EditorChrome.Draw(ChromeMenus);
    DrawAboutPopup();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void FEditor::BuildRenderData()
{
    EditorRenderData = FEditorRenderData{};
    SceneRenderData = FSceneRenderData{};

    BuildSceneView();

    EditorRenderData.SceneView = &SceneView;
    SceneRenderData.SceneView = &SceneView;

    ViewportClient.BuildRenderData(EditorRenderData);

    if (CurScene != nullptr)
    {
        CurScene->BuildRenderData(SceneRenderData);
    }
}
