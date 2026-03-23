#include "Editor.h"

#include "Panel/PanelManager.h"
#include "Viewport/EditorViewportClient.h"

#include "imgui.h"
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

namespace
{
#ifdef IMGUI_HAS_DOCK
    // 중앙 노드를 비워 두면 Scene 배경을 그대로 보면서 패널만 도킹할 수 있습니다.
    constexpr ImGuiDockNodeFlags RootDockSpaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
#endif

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
    // 창 크기 변경을 카메라에도 전달해야 aspect ratio와 투영행렬이 함께 갱신됩니다.
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
    // SceneView는 매 프레임 현재 카메라 캐시와 최신 창 크기 기준으로 다시 조립합니다.
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

    // 커스텀 타이틀바와 겹치지 않도록 도킹 가능한 영역은 그 아래부터 시작합니다.
    const float DockSpaceHeight = (Viewport->Size.y > FEditorChrome::TitleBarHeight)
                                      ? (Viewport->Size.y - FEditorChrome::TitleBarHeight)
                                      : 0.0f;

    ImGui::SetNextWindowPos(
        ImVec2(Viewport->Pos.x, Viewport->Pos.y + FEditorChrome::TitleBarHeight));
    ImGui::SetNextWindowSize(ImVec2(Viewport->Size.x, DockSpaceHeight));
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
        // DockSpace는 호스트가 될 창보다 먼저 제출되어야 이후 패널 창들이 여기로 붙을 수 있습니다.
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
    // 루트 dockspace를 먼저 만들고, 패널과 커스텀 chrome을 그 위에 올립니다.
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
