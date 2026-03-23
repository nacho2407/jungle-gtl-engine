#include "Editor.h"

#include "Panel/PanelManager.h"
#include "Viewport/EditorViewportClient.h"

#include "imgui.h"
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

namespace
{
#ifdef IMGUI_HAS_DOCK
    constexpr ImGuiDockNodeFlags RootDockSpaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
#endif

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

    class FSamplePanel : public IPanel
    {
      public:
        const wchar_t* GetPanelID() const override { return L"SamplePanel"; }
        const wchar_t* GetDisplayName() const override { return L"Sample Panel"; }
        bool ShouldOpenByDefault() const override { return true; }

        void Draw() override
        {
            if (ImGui::Begin("Sample Panel", nullptr))
            {
                ImGui::Text("PanelManager registration test panel");
                ImGui::Separator();
                ImGui::Text("Hello from sample panel");
            }
            ImGui::End();
        }
    };
} // namespace

void FEditor::Create()
{
    EditorContext.Editor = this;

    ViewportClient.Create();

    PanelManager = new FPanelManager();
    PanelManager->Initialize(&EditorContext);
    PanelManager->RegisterPanelInstance<FSamplePanel>();

    CurScene = new FScene();
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

    ChromeHost = nullptr;
    EditorChrome.SetHost(nullptr);
}

void FEditor::Initialize()
{
    if (CurScene == nullptr)
    {
        CurScene = new FScene();
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
    DrawRootDockSpace();

    if (PanelManager != nullptr)
    {
        PanelManager->DrawPanels();
    }

    EditorChrome.Draw();

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
