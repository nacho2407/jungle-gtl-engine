#include "Editor.h"

#include "Panel/PanelManager.h"
#include "Viewport/EditorViewportClient.h"

#include "imgui.h"
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

namespace
{
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

void FEditor::DrawPanel()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();

    ImGui::NewFrame();
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
