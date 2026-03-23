#include "Editor.h"

#include "Viewport/EditorViewportClient.h"
#include "Panel/PanelManager.h"

#include "imgui.h"
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

namespace
{
    class FSamplePanel : public IPanel
    {
      public:
        explicit FSamplePanel(const FEditorLogBuffer* InLogBuffer)
            : LogBuffer(InLogBuffer)
        {
        }

        const wchar_t* GetPanelID() const override { return L"SamplePanel"; }
        const wchar_t* GetDisplayName() const override { return L"Sample Panel"; }
        bool ShouldOpenByDefault() const override { return true; }

        void Draw() override
        {
            if (ImGui::Begin("Sample Panel", nullptr))
            {
                ImGui::Text("PanelManager registration test panel");
                ImGui::Separator();


                if (LogBuffer != nullptr)
                {
                    for (const auto & Log : LogBuffer->GetLogBuffer())
                    {
                        ImGui::Spacing();
                        ImGui::Text("%s", Log.Message.c_str());
                    }
                }
            }
            ImGui::End();
        }

      private:
        const FEditorLogBuffer* LogBuffer = nullptr;
    };
} // namespace

void FEditor::Create()
{
    //  LOG
    GLog = &LogBuffer;
    
    //  TODO : Viewport Client
    ViewportClient.Create();

    //  TODO : Panel UI
    PanelManager = new FPanelManager();
    PanelManager->Initialize(&EditorContext);
    PanelManager->RegisterPanelInstance<FSamplePanel>(&LogBuffer);

    //  TODO : Gizmo
    

    //  TEMP SCENE
    CurScene = new FScene();
    CurScene->AddActor(new AActor())
    
    
    UE_LOG(FEditor, ELogVerbosity::Log, "Hello Editor");
}

void FEditor::Release()
{
    //  TODO : Call Release Functions
    ViewportClient.Release();

    if (PanelManager != nullptr)
    {
        PanelManager->Shutdown();
        delete PanelManager;
        PanelManager = nullptr;
    }

    if (GLog == &LogBuffer)
    {
        GLog = nullptr;
    }
}

void FEditor::Initialize()
{
    CurScene = new FScene();
    //  TODO : Scene의 Begin Play 호출
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
    //DrawPanel();
}

void FEditor::OnWindowResized(float Width, float Height)
{
    if (Width <= 0 || Height <= 0)
    {
        return;
    }

    WindowHeight = Height;
    WindowWidth = Width;
    //  TODO : Setting Panel Size
}

void FEditor::CreateNewScene()
{
    ClearScene();
    //  TODO : 새로운 Scene으로 교체, Panel 초기화,
}

void FEditor::ClearScene()
{
    //  TODO : Scene에 대한 모든 정보 제거
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
    if (PanelManager)
    {
        PanelManager->DrawPanels();
    }

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
